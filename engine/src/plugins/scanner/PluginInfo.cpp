#include "../PluginInventory.h"

namespace ihub::plugins
{
    PluginInfo PluginInfo::fromDescription (const juce::PluginDescription& description)
    {
        PluginInfo info;

        info.id = description.createIdentifierString ().toStdString ();
        info.name = description.name.toStdString ();
        info.description = description.descriptiveName.toStdString ();
        info.vendor = description.manufacturerName.toStdString ();
        info.version = description.version.toStdString ();
        info.category = description.category.toStdString ();
        info.format = description.pluginFormatName.toStdString ();
        info.path = description.fileOrIdentifier.toStdString ();
        info.isInstrument = description.isInstrument;
        info.isEffect = !description.isInstrument;

        if (info.format.empty ())
            info.format = "VST3";

        if (info.category.empty ())
            info.category = info.isInstrument ? "Instrument" : "Fx";

        return info;
    }

    bool PluginInfo::matches (const juce::String& query) const
    {
        const auto needle = query.trim ();

        if (needle.isEmpty ())
            return true;

        const auto matchesText = [&needle] (const std::string& text)
        { return juce::String (text).containsIgnoreCase (needle); };

        return matchesText (name) || matchesText (vendor) || matchesText (version) ||
               matchesText (category) || matchesText (description) || matchesText (format) ||
               matchesText (path) || matchesText (id);
    }

}
