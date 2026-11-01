#include "PluginInstrument.h"

namespace ihub::plugins
{
PluginInstrument::PluginInstrument() = default;

PluginInstrument::~PluginInstrument()
{
    unload();
}

bool PluginInstrument::load (juce::AudioPluginFormatManager& formatManager,
                             const juce::PluginDescription& newDescription,
                             double initialSampleRate,
                             int initialBufferSize,
                             juce::String& error)
{
    error.clear();

    unload();

    sampleRate = initialSampleRate > 0.0 ? initialSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, initialBufferSize);

    auto instance = formatManager.createPluginInstance (newDescription, sampleRate, maximumBlockSize, error);

    if (instance == nullptr)
    {
        if (error.isEmpty())
            error = "createPluginInstance returned null";

        return false;
    }

    plugin = std::move (instance);
    description = newDescription;

    plugin->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);

    configureBuses (numChannels);

    if (plugin->getTotalNumOutputChannels() <= 0)
    {
        error = "plugin exposes no output channels";
        unload();
        return false;
    }

    cachedLatency.store (plugin->getLatencySamples(), std::memory_order_relaxed);

    return true;
}

bool PluginInstrument::loadFromFile (juce::AudioPluginFormatManager& formatManager,
                                     const juce::File& bundle,
                                     double initialSampleRate,
                                     int initialBufferSize,
                                     juce::String& error)
{
    error.clear();

    if (! bundle.exists())
    {
        error = "plugin bundle does not exist: " + bundle.getFullPathName();
        return false;
    }

    const auto path = bundle.getFullPathName();

    juce::OwnedArray<juce::PluginDescription> types;

    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (path))
            format->findAllTypesForFile (types, path);

    if (types.isEmpty())
    {
        error = "no plugin types found in " + path;
        return false;
    }

    return load (formatManager, *types.getFirst(), initialSampleRate, initialBufferSize, error);
}

std::unique_ptr<PluginInstrument> PluginInstrument::create (juce::AudioPluginFormatManager& formatManager,
                                                            const juce::PluginDescription& descriptionToLoad,
                                                            double initialSampleRate,
                                                            int initialBufferSize,
                                                            juce::String& error)
{
    auto instrument = std::make_unique<PluginInstrument>();

    if (! instrument->load (formatManager, descriptionToLoad, initialSampleRate, initialBufferSize, error))
        return nullptr;

    return instrument;
}

void PluginInstrument::unload()
{
    if (beforeUnloadCallback != nullptr)
    {
        auto callback = std::move (beforeUnloadCallback);
        beforeUnloadCallback = nullptr;
        callback();
    }

    plugin.reset();
    description = {};
    cachedLatency.store (0, std::memory_order_relaxed);
    clearPendingMidi();
    midiScratch.clear();
}

void PluginInstrument::setBeforeUnloadCallback (std::function<void()> callback)
{
    beforeUnloadCallback = std::move (callback);
}

juce::String PluginInstrument::getName() const
{
    if (plugin != nullptr)
        return plugin->getName();

    return description.name;
}

}
