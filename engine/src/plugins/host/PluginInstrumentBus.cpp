#include "../PluginInstrument.h"

namespace ihub::plugins
{
    bool PluginInstrument::configureBuses (int channels)
    {
        if (plugin == nullptr)
            return false;

        numChannels = juce::jlimit (1, 2, channels);

        const auto target =
            numChannels == 1 ? juce::AudioChannelSet::mono () : juce::AudioChannelSet::stereo ();

        auto layout = plugin->getBusesLayout ();

        if (layout.outputBuses.isEmpty ())
            return false;

        layout.outputBuses.getReference (0) = target;

        if (!layout.inputBuses.isEmpty ())
        {
            auto& mainInput = layout.inputBuses.getReference (0);

            if (!mainInput.isDisabled ())
                mainInput = target;
        }

        if (plugin->setBusesLayout (layout))
            return true;

        auto fallback = plugin->getBusesLayout ();

        if (fallback.outputBuses.isEmpty ())
            return false;

        fallback.outputBuses.getReference (0) = target;

        return plugin->setBusesLayout (fallback);
    }

    void PluginInstrument::prepare (double newSampleRate, int newMaximumBlockSize)
    {
        jassert (newSampleRate > 0.0);
        jassert (newMaximumBlockSize > 0);

        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        maximumBlockSize = juce::jmax (1, newMaximumBlockSize);

        midiScratch.ensureSize (midiScratchBytes);

        if (plugin == nullptr)
        {
            cachedLatency.store (0, std::memory_order_relaxed);
            return;
        }

        plugin->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
        plugin->prepareToPlay (sampleRate, maximumBlockSize);

        cachedLatency.store (plugin->getLatencySamples (), std::memory_order_relaxed);
    }

    void PluginInstrument::refreshLatency ()
    {
        cachedLatency.store (plugin != nullptr ? plugin->getLatencySamples () : 0,
                             std::memory_order_relaxed);
    }

    int PluginInstrument::latencySamples () const noexcept
    {
        return cachedLatency.load (std::memory_order_relaxed);
    }

}
