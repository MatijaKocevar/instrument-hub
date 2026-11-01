#include "Fixtures.h"

#include "audio/RtGuard.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>

namespace RtGuardTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

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

namespace
{

void* volatile allocationEscapeSink = nullptr;

}

ProbeSource::ProbeSource (Injection injectionToUse)
    : injection (injectionToUse),
      logText ("rt-guard injection test")
{
}

void ProbeSource::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    position = 0;
}

void ProbeSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

Harness::Harness (Injection injection)
    : source (injection)
{
    engine.getSlot (0).setSource (&source);
    engine.getSlot (0).activate();
    engine.prepare (48000.0, blockSize, 2);
    output.setSize (2, blockSize);
}

void Harness::render (int blocks)
{
    for (int block = 0; block < blocks; ++block)
        engine.processBlock (output, midi, blockSize);
}

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

}
