#include "CrashIsolation.h"
#include "TestSupport.h"

#include <atomic>
#include <iostream>

namespace SandboxTest
{

void runCrashIsolation()
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

}
