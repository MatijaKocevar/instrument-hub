#include "../PluginInstrument.h"

namespace ihub::plugins
{
namespace
{
juce::String parameterIdentifier (juce::AudioProcessorParameter& parameter)
{
    // VST3 exposes numeric IDs; that is the only stable identifier the format
    // preserves. APVTS plugins still have readable names, checked as well.
    if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (&parameter))
        return hosted->getParameterID();

    return {};
}
} // namespace

//==============================================================================
int PluginInstrument::getNumParameters() const
{
    return plugin != nullptr ? plugin->getParameters().size() : 0;
}

juce::AudioProcessorParameter* PluginInstrument::findParameter (const juce::String& idOrName) const
{
    if (plugin == nullptr || idOrName.isEmpty())
        return nullptr;

    for (auto* parameter : plugin->getParameters())
    {
        if (parameter == nullptr)
            continue;

        if (parameterIdentifier (*parameter).equalsIgnoreCase (idOrName)
            || parameter->getName (64).equalsIgnoreCase (idOrName))
        {
            return parameter;
        }
    }

    return nullptr;
}

bool PluginInstrument::setParameterValue (const juce::String& idOrName, float normalisedValue)
{
    auto* parameter = findParameter (idOrName);

    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalisedValue));

    return true;
}

float PluginInstrument::getParameterValue (const juce::String& idOrName) const
{
    if (auto* parameter = findParameter (idOrName))
        return parameter->getValue();

    return -1.0f;
}

juce::String PluginInstrument::getParameterText (const juce::String& idOrName) const
{
    if (auto* parameter = findParameter (idOrName))
        return parameter->getText (parameter->getValue(), 64);

    return {};
}

} // namespace ihub::plugins
