// T110 acceptance check: out-of-process plugin hosting with shared-memory
// audio transport.
//
// Three scenarios:
//
//   1. Transport. A fake tone plugin runs in the ihub-sandbox helper. Blocks
//      cross the shared request/reply rings and real audio comes back.
//   2. Crash isolation. The same fake plugin calls abort() on its fourth
//      block. Only the sandbox process dies; SandboxHost reports the slot as
//      failed, a status callback fires once and the AudioEngine keeps
//      rendering another slot at full level.
//   3. Real VST3. TestPlugin is loaded and rendered *inside* the sandbox, so
//      the helper is proven to host a genuine plugin, not just the fake.
//
// The helper path and the VST3 bundle are supplied by CMake as absolute paths.

#include "audio/AudioEngine.h"
#include "plugins/SandboxHost.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <cmath>
#include <iostream>

namespace
{

int failures = 0;

void expect (bool condition, const juce::String& description)
{
    if (condition)
    {
        std::cout << "ok:   " << description << std::endl;
    }
    else
    {
        ++failures;
        std::cerr << "FAIL: " << description << std::endl;
    }
}

float rmsOf (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    int count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = static_cast<double> (samples[sample]);
            sum += value * value;
            ++count;
        }
    }

    return count > 0 ? static_cast<float> (std::sqrt (sum / static_cast<double> (count))) : 0.0f;
}

/** A healthy slot that must survive the sandbox dying. */
class SteadySineSource final : public AudioEngine::SlotSource
{
public:
    void prepare (double sampleRate, int) override
    {
        increment = juce::MathConstants<double>::twoPi * 220.0 / sampleRate;
        phase = 0.0;
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const auto amplitude = 0.2f;

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = amplitude * static_cast<float> (std::sin (phase));
            phase += increment;

            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample (channel, sample, value);
        }
    }

private:
    double phase = 0.0;
    double increment = 0.0;
};

ihub::plugins::SandboxHost::Options makeFakeOptions (bool abortOnPurpose)
{
    ihub::plugins::SandboxHost::Options options;
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.sampleRate = 48000.0;
    options.maxBlockSize = 128;
    options.numChannels = 2;
    options.startupTimeoutMs = 15000;
    options.hangTimeoutMs = 1500;

    options.extraArguments.add ("--fake-tone");
    options.extraArguments.add ("440");

    if (abortOnPurpose)
    {
        options.extraArguments.add ("--abort-after");
        options.extraArguments.add ("3");
    }

    return options;
}

//==============================================================================
void testTransport()
{
    std::cout << "\n-- fake tone through the shared-memory transport --" << std::endl;

    ihub::plugins::SandboxHost host (makeFakeOptions (false));
    juce::String error;

    expect (host.start (error), "sandbox starts: " + error);
    expect (host.getStatus() == ihub::plugins::SandboxHost::Status::running,
            "status is running");
    expect (! host.isFailed(), "healthy sandbox is not failed");

    host.prepare (48000.0, 128);

    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;

    float lastRms = 0.0f;

    for (int block = 0; block < 300 && host.getPlayedBlockCount() < 80; ++block)
    {
        buffer.clear();
        midi.clear();
        host.process (buffer, midi);
        lastRms = rmsOf (buffer);

        juce::Thread::sleep (2); // roughly one 128-frame block at 48 kHz
    }

    expect (host.getPlayedBlockCount() >= 80,
            "received at least 80 rendered blocks (got "
                + juce::String (host.getPlayedBlockCount()) + ")");
    expect (lastRms > 0.05f,
            "returned audio is a live tone (RMS " + juce::String (lastRms, 4) + ")");
    expect (host.getPluginLatencySamples() == 0,
            "fake plugin reports zero latency");
    expect (host.latencySamples() == 128,
            "host reports one block of transport latency on top");
    expect (host.getUnderrunCount() < 40,
            "underruns stay bounded during warm-up (got "
                + juce::String (host.getUnderrunCount()) + ")");

    host.stop();
    expect (host.getStatus() == ihub::plugins::SandboxHost::Status::stopped,
            "status returns to stopped");
    expect (! host.getSharedMemoryFile().existsAsFile(),
            "shared-memory file is removed on stop");
}

//==============================================================================
void testCrashIsolation()
{
    std::cout << "\n-- fake plugin aborts: only the sandbox dies --" << std::endl;

    AudioEngine::AudioEngine engine (2, 2);
    engine.prepare (48000.0, 128, 2);

    SteadySineSource healthySlot;
    healthySlot.prepare (48000.0, 128);

    ihub::plugins::SandboxHost host (makeFakeOptions (true));

    std::atomic<int> callbackCount { 0 };
    std::atomic<int> callbackStatus { -1 };

    host.setStatusCallback ([&callbackCount, &callbackStatus] (
                                ihub::plugins::SandboxHost::Status status,
                                const juce::String&)
                            {
                                callbackStatus.store (static_cast<int> (status),
                                                      std::memory_order_relaxed);
                                callbackCount.fetch_add (1, std::memory_order_relaxed);
                            });

    juce::String error;
    expect (host.start (error), "sandbox starts before the fake plugin aborts: " + error);

    engine.getSlot (0).setSource (&host);
    engine.getSlot (0).activate();
    engine.getSlot (1).setSource (&healthySlot);
    engine.getSlot (1).activate();

    // Slot::setSource does not call prepare(); the engine prepared its slots
    // before a source existed, so prepare the sources exactly once here.
    host.prepare (48000.0, 128);

    juce::AudioBuffer<float> output (2, 128);
    juce::MidiBuffer midi;

    int blocksRendered = 0;

    for (; blocksRendered < 800 && ! host.isFailed(); ++blocksRendered)
    {
        output.clear();
        midi.clear();
        engine.processBlock (output, midi, 128);
        juce::Thread::sleep (2);
    }

    expect (blocksRendered < 800, "the dying sandbox was detected");
    expect (host.isFailed(), "SandboxHost reports the slot as failed");
    expect (host.getStatus() == ihub::plugins::SandboxHost::Status::failed,
            "slot status is failed");
    expect (host.getFailureReason().isNotEmpty(),
            "failure reason is available: '" + host.getFailureReason() + "'");
    expect (callbackCount.load() == 1,
            "status callback fired exactly once (got "
                + juce::String (callbackCount.load()) + ")");
    expect (callbackStatus.load() == static_cast<int> (ihub::plugins::SandboxHost::Status::failed),
            "callback carried the failed status");

    // The audio callback must keep running and the other slot must keep
    // sounding. The failed sandbox slot is silent, not a dead callback.
    float engineRms = 0.0f;

    for (int block = 0; block < 32; ++block)
    {
        output.clear();
        midi.clear();
        engine.processBlock (output, midi, 128);
        engineRms = rmsOf (output);
    }

    expect (engineRms > 0.03f,
            "engine keeps rendering audio after the crash (RMS "
                + juce::String (engineRms, 4) + ")");
    expect (rmsOf (engine.getSlot (0).getOutputBuffer()) < 0.0001f,
            "the failed slot itself is silent");
    expect (host.isRunning() == false, "failed host is no longer running");

    host.stop();
}

//==============================================================================
void testRealPlugin()
{
    std::cout << "\n-- TestPlugin hosted inside the sandbox --" << std::endl;

    ihub::plugins::SandboxHost::Options options;
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.pluginBundle = juce::File (IHUB_TESTPLUGIN_VST3);
    options.sampleRate = 48000.0;
    options.maxBlockSize = 128;
    options.numChannels = 2;
    options.startupTimeoutMs = 20000;

    // TestPlugin's sine generator is off by default; turn it up so the
    // transport carries a signal we can measure.
    options.extraArguments.add ("--param");
    options.extraArguments.add ("Sine Level=1.0");

    ihub::plugins::SandboxHost host (options);
    juce::String error;

    expect (options.pluginBundle.exists(),
            "TestPlugin bundle exists: " + options.pluginBundle.getFullPathName());
    expect (host.start (error), "real VST3 loads inside the sandbox: " + error);

    host.prepare (48000.0, 128);

    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;

    float lastRms = 0.0f;

    for (int block = 0; block < 400 && host.getPlayedBlockCount() < 40; ++block)
    {
        buffer.clear();
        midi.clear();
        host.process (buffer, midi);
        lastRms = rmsOf (buffer);

        juce::Thread::sleep (2);
    }

    expect (host.getPlayedBlockCount() >= 40,
            "real plugin rendered at least 40 blocks (got "
                + juce::String (host.getPlayedBlockCount()) + ")");
    expect (lastRms > 0.02f,
            "real plugin audio arrived through shared memory (RMS "
                + juce::String (lastRms, 4) + ")");
    expect (! host.isFailed(), "host still healthy after rendering TestPlugin");

    host.stop();
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-sandbox-test" << std::endl;
    std::cout << "helper: " << IHUB_SANDBOX_HELPER << std::endl;

    testTransport();
    testCrashIsolation();
    testRealPlugin();

    if (failures == 0)
    {
        std::cout << "\nPASS: sandbox transport, crash isolation and real plugin hosting"
                  << std::endl;
        return 0;
    }

    std::cerr << "\n" << failures << " check(s) failed" << std::endl;
    return 1;
}
