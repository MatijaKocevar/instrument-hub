#include "../PluginInstrument.h"

namespace ihub::plugins
{
juce::MemoryBlock PluginInstrument::getStateInformation() const
{
    juce::MemoryBlock state;
    getStateInformation (state);
    return state;
}

void PluginInstrument::getStateInformation (juce::MemoryBlock& destination) const
{
    const std::lock_guard<std::mutex> lock (stateMutex);

    destination.reset();

    if (plugin != nullptr)
    {
        suppressNotifications.store (true, std::memory_order_release);
        plugin->getStateInformation (destination);
        suppressNotifications.store (false, std::memory_order_release);
    }
}

bool PluginInstrument::setStateInformation (const void* data, int numBytes)
{
    if (plugin == nullptr || data == nullptr || numBytes <= 0)
        return false;

    {
        const std::lock_guard<std::mutex> lock (stateMutex);

        suppressNotifications.store (true, std::memory_order_release);
        plugin->setStateInformation (data, numBytes);
        suppressNotifications.store (false, std::memory_order_release);
    }

    notifyStateChanged();

    return true;
}

bool PluginInstrument::setStateInformation (const juce::MemoryBlock& block)
{
    return setStateInformation (block.getData(), static_cast<int> (block.getSize()));
}

}
