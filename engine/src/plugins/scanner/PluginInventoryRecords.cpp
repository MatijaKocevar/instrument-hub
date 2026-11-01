#include "PluginInventoryRecords.h"

#include "../Scanner.h"

#include <set>
#include <utility>

namespace ihub::plugins::inventory_detail
{
namespace
{
std::string keyFor (const PluginInfo& info)
{
    return info.path + "|" + info.id;
}
} // namespace

std::vector<PluginInfo> toPluginInfos (const std::vector<juce::PluginDescription>& descriptions)
{
    std::vector<PluginInfo> result;
    std::set<std::string> seen;

    result.reserve (descriptions.size());

    for (const auto& description : descriptions)
    {
        auto info = PluginInfo::fromDescription (description);

        if (info.name.empty() && info.path.empty())
            continue;

        if (seen.insert (keyFor (info)).second)
            result.push_back (std::move (info));
    }

    return result;
}

std::vector<PluginInfo> pluginsFromReport (const juce::File& dataDirectory)
{
    const auto report = dataDirectory.getChildFile (Scanner::reportFileName);

    if (! report.existsAsFile())
        return {};

    const auto xml = juce::XmlDocument::parse (report);

    if (xml == nullptr)
        return {};

    juce::KnownPluginList list;
    list.recreateFromXml (*xml);

    std::vector<juce::PluginDescription> existing;

    for (const auto& description : list.getTypes())
        if (juce::File (description.fileOrIdentifier).exists())
            existing.push_back (description);

    return toPluginInfos (existing);
}

} // namespace ihub::plugins::inventory_detail
