#include "FxChainJson.h"

#include "JsonSupport.h"

namespace ApiTest
{

int insertCount (const juce::var& chain)
{
    const auto* inserts = arrayProperty (chain, "inserts");
    return inserts != nullptr ? inserts->size() : -1;
}

std::string insertType (const juce::var& chain, int index)
{
    const auto* inserts = arrayProperty (chain, "inserts");

    if (inserts == nullptr || index < 0 || index >= inserts->size())
        return {};

    return property ((*inserts)[index], "type").toString().toStdString();
}

int chainLatency (const juce::var& chain)
{
    return intProperty (chain, "latencySamples");
}

} // namespace ApiTest
