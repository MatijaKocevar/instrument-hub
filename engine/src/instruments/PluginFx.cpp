#include "PluginFx.h"

#include "../plugins/PluginWindow.h"

#include <algorithm>
#include <cmath>

namespace ihub::plugins
{
    namespace
    {
        juce::String parameterIdentifier (juce::AudioProcessorParameter& parameter)
        {
            if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (&parameter))
                return hosted->getParameterID ();

            return {};
        }
    }

    PluginFx::PluginFx () = default;

    PluginFx::~PluginFx ()
    {
        unload ();
    }

    bool PluginFx::load (juce::AudioPluginFormatManager& formatManager,
                         const juce::PluginDescription& newDescription,
                         double initialSampleRate,
                         int initialBufferSize,
                         juce::String& error)
    {
        error.clear ();

        unload ();

        sampleRate = initialSampleRate > 0.0 ? initialSampleRate : 48000.0;
        maximumBlockSize = juce::jmax (1, initialBufferSize);

        auto instance = formatManager.createPluginInstance (
            newDescription, sampleRate, maximumBlockSize, error);

        if (instance == nullptr)
        {
            if (error.isEmpty ())
                error = "createPluginInstance returned null";

            return false;
        }

        plugin = std::move (instance);
        description = newDescription;

        plugin->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);

        if (!configureBuses (numChannels))
        {
            error = "plugin does not provide an FX bus layout (main input + output)";
            unload ();
            return false;
        }

        cachedLatency.store (plugin->getLatencySamples (), std::memory_order_relaxed);

        return true;
    }

    bool PluginFx::loadFromFile (juce::AudioPluginFormatManager& formatManager,
                                 const juce::File& bundle,
                                 double initialSampleRate,
                                 int initialBufferSize,
                                 juce::String& error)
    {
        error.clear ();

        if (!bundle.exists ())
        {
            error = "plugin bundle does not exist: " + bundle.getFullPathName ();
            return false;
        }

        const auto path = bundle.getFullPathName ();

        juce::OwnedArray<juce::PluginDescription> types;

        for (auto* format : formatManager.getFormats ())
            if (format->fileMightContainThisPluginType (path))
                format->findAllTypesForFile (types, path);

        if (types.isEmpty ())
        {
            error = "no plugin types found in " + path;
            return false;
        }

        return load (
            formatManager, *types.getFirst (), initialSampleRate, initialBufferSize, error);
    }

    void PluginFx::unload ()
    {
        closeEditor ();

        plugin.reset ();
        description = {};
        cachedLatency.store (0, std::memory_order_relaxed);
    }

    bool PluginFx::configureBuses (int channels)
    {
        if (plugin == nullptr)
            return false;

        numChannels = juce::jlimit (1, 2, channels);

        const auto apply = [this] (const juce::AudioChannelSet& set)
        {
            auto layout = plugin->getBusesLayout ();

            if (layout.inputBuses.isEmpty () || layout.outputBuses.isEmpty ())
                return false;

            layout.inputBuses.getReference (0) = set;
            layout.outputBuses.getReference (0) = set;

            if (!plugin->setBusesLayout (layout))
                return false;

            return plugin->getTotalNumInputChannels () > 0 &&
                   plugin->getTotalNumOutputChannels () > 0;
        };

        const auto requested =
            numChannels == 1 ? juce::AudioChannelSet::mono () : juce::AudioChannelSet::stereo ();

        if (apply (requested))
            return true;

        if (numChannels != 1 && apply (juce::AudioChannelSet::mono ()))
            return true;

        return false;
    }

    bool PluginFx::isFxMode () const noexcept
    {
        return plugin != nullptr && plugin->getTotalNumInputChannels () > 0 &&
               plugin->getTotalNumOutputChannels () > 0;
    }

    int PluginFx::getNumInputChannels () const noexcept
    {
        return plugin != nullptr ? plugin->getTotalNumInputChannels () : 0;
    }

    int PluginFx::getNumOutputChannels () const noexcept
    {
        return plugin != nullptr ? plugin->getTotalNumOutputChannels () : 0;
    }

    void PluginFx::prepare (double newSampleRate, int newMaximumBlockSize)
    {
        jassert (newSampleRate > 0.0);
        jassert (newMaximumBlockSize > 0);

        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        maximumBlockSize = juce::jmax (1, newMaximumBlockSize);

        if (plugin == nullptr)
        {
            cachedLatency.store (0, std::memory_order_relaxed);
            return;
        }

        plugin->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
        plugin->prepareToPlay (sampleRate, maximumBlockSize);

        refreshLatency ();
    }

    void PluginFx::releaseResources ()
    {
        if (plugin != nullptr)
            plugin->releaseResources ();

        cachedLatency.store (0, std::memory_order_relaxed);
    }

    void PluginFx::refreshLatency () noexcept
    {
        cachedLatency.store (plugin != nullptr ? plugin->getLatencySamples () : 0,
                             std::memory_order_relaxed);
    }

    int PluginFx::latencySamples () const noexcept
    {
        return cachedLatency.load (std::memory_order_relaxed);
    }

    juce::AudioProcessor* PluginFx::getProcessor () const noexcept
    {
        return plugin.get ();
    }

    int PluginFx::addToChain (AudioEngine::FxChain& chain)
    {
        return chain.add (getProcessor ());
    }

    bool PluginFx::removeFromChain (AudioEngine::FxChain& chain) noexcept
    {
        const auto index = chain.indexOf (getProcessor ());

        if (index < 0)
            return false;

        return chain.remove (index);
    }

    bool PluginFx::isInChain (const AudioEngine::FxChain& chain) const noexcept
    {
        return chain.indexOf (getProcessor ()) >= 0;
    }

    void PluginFx::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        if (plugin == nullptr)
        {
            buffer.clear ();
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
            return;
        }

        if (pluginChannels == bufferChannels)
        {
            plugin->processBlock (buffer, midi);
        }
        else
        {
            juce::AudioBuffer<float> view (
                buffer.getArrayOfWritePointers (), pluginChannels, numSamples);
            plugin->processBlock (view, midi);
        }
    }

    juce::MemoryBlock PluginFx::getStateInformation () const
    {
        juce::MemoryBlock state;
        getStateInformation (state);
        return state;
    }

    void PluginFx::getStateInformation (juce::MemoryBlock& destination) const
    {
        destination.reset ();

        if (plugin != nullptr)
            plugin->getStateInformation (destination);
    }

    bool PluginFx::setStateInformation (const void* data, int numBytes)
    {
        if (plugin == nullptr || data == nullptr || numBytes <= 0)
            return false;

        plugin->setStateInformation (data, numBytes);

        refreshLatency ();

        return true;
    }

    bool PluginFx::setStateInformation (const juce::MemoryBlock& block)
    {
        return setStateInformation (block.getData (), static_cast<int> (block.getSize ()));
    }

    int PluginFx::getNumParameters () const
    {
        return plugin != nullptr ? plugin->getParameters ().size () : 0;
    }

    juce::AudioProcessorParameter* PluginFx::findParameter (const juce::String& idOrName) const
    {
        if (plugin == nullptr || idOrName.isEmpty ())
            return nullptr;

        for (auto* parameter : plugin->getParameters ())
        {
            if (parameter == nullptr)
                continue;

            if (parameterIdentifier (*parameter).equalsIgnoreCase (idOrName) ||
                parameter->getName (64).equalsIgnoreCase (idOrName))
            {
                return parameter;
            }
        }

        return nullptr;
    }

    bool PluginFx::setParameterValue (const juce::String& idOrName, float normalisedValue)
    {
        auto* parameter = findParameter (idOrName);

        if (parameter == nullptr)
            return false;

        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalisedValue));

        return true;
    }

    float PluginFx::getParameterValue (const juce::String& idOrName) const
    {
        if (auto* parameter = findParameter (idOrName))
            return parameter->getValue ();

        return -1.0f;
    }

    juce::String PluginFx::getParameterText (const juce::String& idOrName) const
    {
        if (auto* parameter = findParameter (idOrName))
            return parameter->getText (parameter->getValue (), 64);

        return {};
    }

    bool PluginFx::openEditor (const juce::String& title, juce::Component* owner)
    {
        if (plugin == nullptr)
            return false;

        if (editorWindow == nullptr)
            editorWindow = std::make_unique<PluginWindow> ();

        return editorWindow->open (*plugin, title, owner);
    }

    void PluginFx::closeEditor ()
    {
        if (editorWindow != nullptr)
            editorWindow->close ();
    }

    bool PluginFx::isEditorOpen () const noexcept
    {
        return editorWindow != nullptr && editorWindow->isOpen ();
    }

    juce::AudioProcessorEditor* PluginFx::getEditor () const noexcept
    {
        return editorWindow != nullptr ? editorWindow->getEditor () : nullptr;
    }

    juce::Component* PluginFx::getEditorWindow () const noexcept
    {
        return editorWindow != nullptr ? editorWindow->getWindowComponent () : nullptr;
    }

    juce::String PluginFx::getName () const
    {
        if (plugin != nullptr)
            return plugin->getName ();

        return description.name;
    }

}
