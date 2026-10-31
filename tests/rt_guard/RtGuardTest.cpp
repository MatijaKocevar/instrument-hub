// ihub-rt-guard-test: acceptance check for T014.
//
// 1. Renders 200 blocks through AudioEngine on the normal path in strict mode
//    and requires zero violations. A violation would abort the process, so
//    reaching the end is itself part of the assertion.
// 2. Proves the counters are live: allocation, lock and logger injections are
//    counted when strict mode is off.
// 3. Forks a child per injection and requires strict mode to kill it with
//    SIGABRT, with the diagnostic naming the violation.
// 4. Checks the LeakedObjectDetector-style scope bookkeeping and that a
//    disabled guard ignores violations.
//
// The forks keep the deliberate aborts out of the test process, so a failure
// reads as "strict did not abort" instead of "the test binary crashed".

#include "audio/AudioEngine.h"
#include "audio/RtGuard.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

//==============================================================================
enum class Injection
{
    none,
    allocation,
    lock,
    log
};

const char* toString (Injection injection) noexcept
{
    switch (injection)
    {
        case Injection::none:       return "none";
        case Injection::allocation: return "allocation";
        case Injection::lock:       return "lock";
        case Injection::log:        return "log";
    }

    return "none";
}

// volatile pointer (not pointer-to-volatile): the store itself is observable
void* volatile allocationEscapeSink = nullptr;

/** A sine source that can break one rule of the audio-thread contract. */
class ProbeSource final : public AudioEngine::SlotSource
{
public:
    explicit ProbeSource (Injection injectionToUse)
        : injection (injectionToUse),
          logText ("rt-guard injection test")
    {
    }

    void prepare (double newSampleRate, int) override
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        position = 0;
    }

    void setArmed (bool shouldBeArmed) noexcept { armed = shouldBeArmed; }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int samples = buffer.getNumSamples();
        const int channels = buffer.getNumChannels();

        const double increment = juce::MathConstants<double>::twoPi * 440.0 / sampleRate;
        const double start = increment * static_cast<double> (position);

        for (int i = 0; i < samples; ++i)
        {
            const auto value = static_cast<float> (0.1 * std::sin (start + increment
                                                                   * static_cast<double> (i)));

            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer (channel)[i] = value;
        }

        position += samples;

        if (! armed)
            return;

        switch (injection)
        {
            case Injection::none:
                break;

            case Injection::allocation:
            {
                auto* scratch = new float[8];
                scratch[0] = static_cast<float> (position);

                // Publish the pointer through a volatile sink so the optimiser
                // cannot reduce new/delete to a stack buffer at -O2.
                allocationEscapeSink = scratch;
                delete[] scratch;
                allocationEscapeSink = nullptr;
                break;
            }

            case Injection::lock:
            {
                std::mutex mutex;
                const std::lock_guard<std::mutex> lock (mutex);
                break;
            }

            case Injection::log:
                juce::Logger::writeToLog (logText);
                break;
        }
    }

private:
    Injection injection;
    juce::String logText;

    double sampleRate = 48000.0;
    std::int64_t position = 0;
    bool armed = false;
};

/** Engine + probe + pre-allocated buffers, wired exactly like the app. */
struct Harness
{
    explicit Harness (Injection injection)
        : source (injection)
    {
        engine.getSlot (0).setSource (&source);
        engine.getSlot (0).activate();
        engine.prepare (48000.0, blockSize, 2);
        output.setSize (2, blockSize);
    }

    void render (int blocks)
    {
        for (int block = 0; block < blocks; ++block)
            engine.processBlock (output, midi, blockSize);
    }

    static constexpr int blockSize = 128;

    AudioEngine::AudioEngine engine { 2, 2 };
    ProbeSource source;
    juce::AudioBuffer<float> output;
    juce::MidiBuffer midi;
};

//==============================================================================
void testGuardIsCompiledIn()
{
    std::cout << "\n-- guard is compiled in --" << std::endl;

    check (AudioEngine::RtGuard::isCompiledIn(),
           "RtGuard::isCompiledIn() reports the harness is active");
    check (AudioEngine::RtGuard::isEnabled(), "guard is enabled by default");
    check (! AudioEngine::RtGuard::isStrict(), "guard is not strict by default");
}

void testNormalPathIsClean()
{
    std::cout << "\n-- 200 blocks on the normal path are clean --" << std::endl;

    AudioEngine::RtGuard::setEnabled (true);
    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::resetCounters();

    {
        Harness harness (Injection::none);
        harness.source.setArmed (false);

        // Warm-up: the activation ramp and any first-use initialisation.
        harness.render (8);
        const auto warmup = AudioEngine::RtGuard::getCounters();

        AudioEngine::RtGuard::resetCounters();
        AudioEngine::RtGuard::setStrict (true);

        harness.render (200);

        AudioEngine::RtGuard::setStrict (false);
        const auto measured = AudioEngine::RtGuard::getCounters();

        std::cout << "warmup violations=" << warmup.total()
                  << ", measured violations=" << measured.total() << std::endl;

        check (warmup.total() == 0, "no violation during the activation ramp");
        check (measured.total() == 0, "no violation across 200 strict blocks");
        check (AudioEngine::RtGuard::getOpenScopeCount() == 0,
               "every processBlock scope was closed");
        check (AudioEngine::RtGuard::assertNoOpenScopes(),
               "LeakedObjectDetector-style scope check passes");
    }

    check (! AudioEngine::RtGuard::isInsideAudioScope(),
           "the test thread is outside the audio scope again");
}

void testScopeBookkeeping()
{
    std::cout << "\n-- scope bookkeeping --" << std::endl;

    check (AudioEngine::RtGuard::getThreadScopeDepth() == 0,
           "scope depth starts at zero");
    check (AudioEngine::RtGuard::getOpenScopeCount() == 0,
           "no scopes are open before the test");

    {
        const AudioEngine::RtGuard::Scope outer;

        check (AudioEngine::RtGuard::isInsideAudioScope(),
               "inside a scope the thread is marked realtime");
        check (AudioEngine::RtGuard::getThreadScopeDepth() == 1,
               "outer scope reports depth 1");
        check (AudioEngine::RtGuard::getOpenScopeCount() == 1,
               "outer scope is counted");

        {
            const AudioEngine::RtGuard::Scope inner;

            check (AudioEngine::RtGuard::getThreadScopeDepth() == 2,
                   "nested scope reports depth 2");
            check (AudioEngine::RtGuard::getOpenScopeCount() == 2,
                   "nested scope is counted");
        }

        check (AudioEngine::RtGuard::getThreadScopeDepth() == 1,
               "depth unwinds to 1");
        check (AudioEngine::RtGuard::getOpenScopeCount() == 1,
               "nested scope was released");
    }

    check (AudioEngine::RtGuard::getThreadScopeDepth() == 0,
           "depth unwinds to 0");
    check (AudioEngine::RtGuard::getOpenScopeCount() == 0,
           "all scopes were released");
    check (! AudioEngine::RtGuard::isInsideAudioScope(),
           "thread is no longer realtime");
}

void testNonStrictCountsViolations()
{
    std::cout << "\n-- non-strict mode counts violations without aborting --" << std::endl;

    AudioEngine::RtGuard::setEnabled (true);
    AudioEngine::RtGuard::setStrict (false);

    struct Case
    {
        Injection injection;
        const char* label;
    };

    const Case cases[] = {
        { Injection::allocation, "operator new" },
        { Injection::lock, "pthread_mutex_lock" },
        { Injection::log, "juce::Logger" },
    };

    for (const auto& testCase : cases)
    {
        AudioEngine::RtGuard::resetCounters();

        if (testCase.injection == Injection::log)
            AudioEngine::RtGuard::installLogProbe();

        {
            Harness harness (testCase.injection);
            harness.source.setArmed (true);
            harness.render (3);
        }

        if (testCase.injection == Injection::log)
            AudioEngine::RtGuard::uninstallLogProbe();

        const auto counters = AudioEngine::RtGuard::getCounters();

        switch (testCase.injection)
        {
            case Injection::allocation:
                check (counters.allocations >= 3,
                       std::string ("allocations counted for ") + testCase.label);
                break;
            case Injection::lock:
                check (counters.locks >= 3,
                       std::string ("locks counted for ") + testCase.label);
                break;
            case Injection::log:
                check (counters.logs >= 3,
                       std::string ("logs counted for ") + testCase.label);
                break;
            case Injection::none:
                break;
        }

        std::cout << "  " << toString (testCase.injection) << ": alloc="
                  << counters.allocations << " lock=" << counters.locks
                  << " log=" << counters.logs << " scope="
                  << counters.scopeErrors << std::endl;
    }

    AudioEngine::RtGuard::resetCounters();
}

//==============================================================================
struct AbortResult
{
    bool aborted = false;
    std::string output;
};

/** Runs one injection in a forked child under strict mode.

    The child's stderr is captured through a pipe, so the test can assert the
    diagnostic names the violation as well as the signal.
*/
AbortResult runStrictChild (Injection injection)
{
    int pipeFds[2];

    if (::pipe (pipeFds) != 0)
    {
        std::perror ("pipe");
        return {};
    }

    std::cout.flush();
    std::cerr.flush();
    std::fflush (stdout);
    std::fflush (stderr);

    const pid_t pid = ::fork();

    if (pid < 0)
    {
        std::perror ("fork");
        ::close (pipeFds[0]);
        ::close (pipeFds[1]);
        return {};
    }

    if (pid == 0)
    {
        ::close (pipeFds[0]);
        ::dup2 (pipeFds[1], STDERR_FILENO);
        ::close (pipeFds[1]);

        AudioEngine::RtGuard::setEnabled (true);
        AudioEngine::RtGuard::setStrict (true);
        AudioEngine::RtGuard::installLogProbe();

        {
            Harness harness (injection);
            harness.source.setArmed (true);
            harness.render (4);
        }

        // Reached only when strict mode failed to abort.
        AudioEngine::RtGuard::setStrict (false);
        AudioEngine::RtGuard::uninstallLogProbe();
        std::_Exit (0);
    }

    ::close (pipeFds[1]);

    AbortResult result;
    char buffer[512];

    for (;;)
    {
        const ssize_t count = ::read (pipeFds[0], buffer, sizeof (buffer));

        if (count > 0)
            result.output.append (buffer, static_cast<std::size_t> (count));
        else if (count == 0)
            break;
        else if (errno != EINTR)
            break;
    }

    ::close (pipeFds[0]);

    int status = 0;

    if (::waitpid (pid, &status, 0) == pid)
        result.aborted = WIFSIGNALED (status) && WTERMSIG (status) == SIGABRT;

    return result;
}

void testStrictAbortsOnInjection()
{
    std::cout << "\n-- strict mode aborts on injected violations --" << std::endl;

    struct Case
    {
        Injection injection;
        const char* label;
        const char* needle;
    };

    const Case cases[] = {
        { Injection::allocation, "new inside the callback", "rt-guard: FATAL allocation" },
        { Injection::lock, "std::mutex lock inside the callback", "rt-guard: FATAL lock" },
        { Injection::log, "juce::Logger write inside the callback", "rt-guard: FATAL log" },
    };

    for (const auto& testCase : cases)
    {
        const auto result = runStrictChild (testCase.injection);

        check (result.aborted,
               std::string ("SIGABRT on ") + testCase.label);

        const bool named = result.output.find ("rt-guard: FATAL") != std::string::npos
                           && result.output.find (testCase.needle) != std::string::npos;

        check (named,
               std::string ("diagnostic names the violation for ") + testCase.label);

        if (! result.aborted || ! named)
            std::cout << "  child stderr: " << result.output << std::endl;
    }
}

void testDisabledGuardIgnoresViolations()
{
    std::cout << "\n-- a disabled guard ignores violations --" << std::endl;

    AudioEngine::RtGuard::resetCounters();
    AudioEngine::RtGuard::setEnabled (false);
    AudioEngine::RtGuard::setStrict (true);

    {
        Harness harness (Injection::allocation);
        harness.source.setArmed (true);
        harness.render (3);
    }

    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::setEnabled (true);

    check (AudioEngine::RtGuard::getViolationCount() == 0,
           "no violation is recorded while the guard is disabled");
}
} // namespace

//==============================================================================
int main()
{
    std::cout << "ihub-rt-guard-test: T014 realtime safety harness" << std::endl;

    testGuardIsCompiledIn();
    testScopeBookkeeping();
    testNormalPathIsClean();
    testNonStrictCountsViolations();
    testStrictAbortsOnInjection();
    testDisabledGuardIgnoresViolations();

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL")
              << ": ihub-rt-guard (" << failures << " failure(s))" << std::endl;

    return failures == 0 ? 0 : 1;
}
