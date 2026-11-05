#include "PageHostScenario.h"

#include "TestSupport.h"
#include "plugins/PageHost.h"

#include <vector>

namespace SandboxTest
{
void runPageHostTransport()
{
    ihub::plugins::PageHost::Options options;
    options.pageId = "transport-test";
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.startupTimeoutMs = 15000;
    options.hangTimeoutMs = 2000;
    options.maxBlockSize = 128;

    const double frequencies[] { 220.0, 330.0, 440.0 };

    for (int slot = 0; slot < 3; ++slot)
    {
        ihub::plugins::PageSlotDescriptor descriptor;
        descriptor.slotIndex = slot;
        descriptor.fakeToneFrequency = frequencies[slot];
        options.slots.push_back (descriptor);
    }

    auto host = ihub::plugins::PageHost::create (std::move (options));
    juce::String error;

    expect (host->start (error), "the page host starts (" + error + ")");
    expect (host->isReady(), "the page host reports ready");
    expect (host->getChildProcessId() > 0, "the page host has one child process");

    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;

    for (int slot = 0; slot < 3; ++slot)
    {
        const auto index = host->findInternalIndex (slot);
        expect (index >= 0, "page slot " + juce::String (slot) + " maps to a channel");

        bool producedAudio = false;

        for (int block = 0; block < 200; ++block)
        {
            buffer.clear();
            midi.clear();
            host->processSlot (slot, index, buffer, midi);

            if (rmsOf (buffer) > 0.01f)
                producedAudio = true;

            if (host->getSlotPlayedBlockCount (slot) >= 40)
                break;

            juce::Thread::sleep (1);
        }

        expect (producedAudio, "page slot " + juce::String (slot) + " produces audio");
        expect (host->getSlotPlayedBlockCount (slot) >= 40,
                "page slot " + juce::String (slot) + " keeps rendering");
        expect (! host->getSlotStatus (slot).failed,
                "page slot " + juce::String (slot) + " is healthy");
    }

    expect (host->getSlotIndices().size() == 3, "the page keeps an arbitrary slot count");

    host->shutdown();
    expect (host->getStatus() == ihub::plugins::PageHost::Status::stopped,
            "the page host stops cleanly");
}
}
