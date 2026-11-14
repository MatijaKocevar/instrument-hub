#include "../PluginInstrument.h"

namespace ihub::plugins
{
    void PluginInstrument::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        if (plugin == nullptr)
        {
            buffer.clear ();
            clearPendingMidi ();
            return;
        }

        const auto numSamples = buffer.getNumSamples ();

        if (numSamples <= 0)
            return;

        const auto bufferChannels = buffer.getNumChannels ();
        const auto pluginChannels =
            juce::jmax (plugin->getTotalNumInputChannels (), plugin->getTotalNumOutputChannels ());

        if (pluginChannels <= 0 || pluginChannels > bufferChannels)
        {
            buffer.clear ();
            clearPendingMidi ();
            return;
        }

        juce::MidiBuffer* midiToUse = &midi;

        const auto read = pendingReadIndex.load (std::memory_order_relaxed);
        const auto write = pendingWriteIndex.load (std::memory_order_acquire);

        if (read != write)
        {
            midiScratch.clear ();
            midiScratch.addEvents (midi, 0, numSamples, 0);
            drainPendingMidi (midiScratch);
            midiToUse = &midiScratch;
        }

        if (pluginChannels == bufferChannels)
        {
            plugin->processBlock (buffer, *midiToUse);
        }
        else
        {
            juce::AudioBuffer<float> view (
                buffer.getArrayOfWritePointers (), pluginChannels, numSamples);
            plugin->processBlock (view, *midiToUse);
        }
    }

}
