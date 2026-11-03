#include "../PluginInstrument.h"

#include <utility>

namespace ihub::plugins
{
class PluginInstrument::StateChangeForwarder final : public juce::AudioProcessorListener
{
public:
    explicit StateChangeForwarder (PluginInstrument& ownerToUse)
        : owner (ownerToUse)
    {
    }

    void audioProcessorChanged (juce::AudioProcessor*,
                                const ChangeDetails&) override
    {
        owner.notifyStateChanged();
    }

    void audioProcessorParameterChanged (juce::AudioProcessor*,
                                         int parameterIndex,
                                         float newValue) override
    {
        owner.notifyParameterChanged (parameterIndex, newValue);
    }

private:
    PluginInstrument& owner;
};

PluginInstrument::PluginInstrument() = default;

PluginInstrument::~PluginInstrument()
{
    unload();
}

void PluginInstrument::unload()
{
    if (beforeUnloadCallback != nullptr)
    {
        auto callback = std::move (beforeUnloadCallback);
        beforeUnloadCallback = nullptr;
        callback();
    }

    if (stateChangeForwarder != nullptr && plugin != nullptr)
        plugin->removeListener (stateChangeForwarder.get());

    stateChangeForwarder.reset();
    parameterChangedCallback = nullptr;
    stateChangedCallback = nullptr;

    plugin.reset();
    description = {};
    cachedLatency.store (0, std::memory_order_relaxed);
    clearPendingMidi();
    midiScratch.clear();
}

void PluginInstrument::setParameterChangedCallback (ParameterChangedCallback callback)
{
    parameterChangedCallback = std::move (callback);
    refreshListener();
}

void PluginInstrument::setStateChangedCallback (std::function<void()> callback)
{
    stateChangedCallback = std::move (callback);
    refreshListener();
}

void PluginInstrument::refreshListener()
{
    const bool wantsListener = parameterChangedCallback != nullptr
                            || stateChangedCallback != nullptr;

    if (! wantsListener)
    {
        if (stateChangeForwarder != nullptr && plugin != nullptr)
            plugin->removeListener (stateChangeForwarder.get());

        stateChangeForwarder.reset();
        return;
    }

    if (plugin != nullptr && stateChangeForwarder == nullptr)
    {
        stateChangeForwarder = std::make_unique<StateChangeForwarder> (*this);
        plugin->addListener (stateChangeForwarder.get());
    }
}

void PluginInstrument::notifyParameterChanged (int parameterIndex,
                                               float normalisedValue)
{
    if (suppressNotifications.load (std::memory_order_acquire)
        || parameterChangedCallback == nullptr)
        return;

    const auto id = getParameterId (parameterIndex);

    if (id.isEmpty())
        return;

    parameterChangedCallback (id, juce::jlimit (0.0f, 1.0f, normalisedValue));
}

void PluginInstrument::notifyStateChanged()
{
    if (suppressNotifications.load (std::memory_order_acquire))
        return;

    if (stateChangedCallback != nullptr)
        stateChangedCallback();
}

}
