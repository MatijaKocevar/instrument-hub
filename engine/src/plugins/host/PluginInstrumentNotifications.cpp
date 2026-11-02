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
                                         int,
                                         float) override
    {
        owner.notifyStateChanged();
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
    stateChangedCallback = nullptr;

    plugin.reset();
    description = {};
    cachedLatency.store (0, std::memory_order_relaxed);
    clearPendingMidi();
    midiScratch.clear();
}

void PluginInstrument::setStateChangedCallback (std::function<void()> callback)
{
    stateChangedCallback = std::move (callback);

    if (stateChangedCallback == nullptr)
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

void PluginInstrument::notifyStateChanged()
{
    if (stateChangedCallback != nullptr)
        stateChangedCallback();
}

}
