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
    destination.reset();

    if (plugin != nullptr)
        plugin->getStateInformation (destination);
}

bool PluginInstrument::setStateInformation (const void* data, int numBytes)
{
    if (plugin == nullptr || data == nullptr || numBytes <= 0)
        return false;

    plugin->setStateInformation (data, numBytes);
    notifyStateChanged();

    return true;
}

bool PluginInstrument::setStateInformation (const juce::MemoryBlock& block)
{
    return setStateInformation (block.getData(), static_cast<int> (block.getSize()));
}

}
