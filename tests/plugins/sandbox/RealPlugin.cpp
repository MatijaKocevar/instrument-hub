#include "RealPlugin.h"
#include "TestSupport.h"

#include <iostream>

namespace SandboxTest
{

void runRealPlugin()
{
    std::cout << "\n-- TestPlugin hosted inside the sandbox --" << std::endl;

    ihub::plugins::SandboxHost::Options options;
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.pluginBundle = juce::File (IHUB_TESTPLUGIN_VST3);
    options.sampleRate = 48000.0;
    options.maxBlockSize = 128;
    options.numChannels = 2;
    options.startupTimeoutMs = 20000;

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

} // namespace SandboxTest
