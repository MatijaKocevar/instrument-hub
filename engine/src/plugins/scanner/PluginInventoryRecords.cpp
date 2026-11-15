#include "PluginInventoryRecords.h"

#include "../Scanner.h"

#include <algorithm>
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

        std::string identityFor (const juce::PluginDescription& description)
        {
            const auto uid =
                description.uniqueId != 0 ? description.uniqueId : description.deprecatedUid;

            return description.pluginFormatName.toStdString () + "|" +
                   description.name.toStdString () + "|" + std::to_string (uid);
        }

    }

    std::vector<PluginInfo> toPluginInfos (const std::vector<juce::PluginDescription>& descriptions)
    {
        std::vector<PluginInfo> result;
        std::vector<std::string> identities;
        std::set<std::string> seen;

        result.reserve (descriptions.size ());
        identities.reserve (descriptions.size ());

        for (const auto& description : descriptions)
        {
            auto info = PluginInfo::fromDescription (description);

            if (info.name.empty () && info.path.empty ())
                continue;

            if (!seen.insert (keyFor (info)).second)
                continue;

            const auto identity = identityFor (description);

            if (std::find (identities.begin (), identities.end (), identity) != identities.end ())
                continue;

            identities.push_back (identity);
            result.push_back (std::move (info));
        }

        return result;
    }

    std::vector<PluginInfo> pluginsFromReport (const juce::File& dataDirectory)
    {
        const auto report = dataDirectory.getChildFile (Scanner::reportFileName);

        if (!report.existsAsFile ())
            return {};

        const auto xml = juce::XmlDocument::parse (report);

        if (xml == nullptr)
            return {};

        juce::KnownPluginList list;
        list.recreateFromXml (*xml);

        std::vector<juce::PluginDescription> existing;

        for (const auto& description : list.getTypes ())
            if (juce::File (description.fileOrIdentifier).exists ())
                existing.push_back (description);

        return toPluginInfos (existing);
    }

}
