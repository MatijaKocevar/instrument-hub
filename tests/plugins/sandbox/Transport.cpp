#include "Transport.h"
#include "TestSupport.h"

#include <iostream>

namespace SandboxTest
{

void runTransport()
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

        juce::Thread::sleep (2);
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

}
