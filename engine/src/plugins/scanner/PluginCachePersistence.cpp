#include "../PluginCache.h"

namespace ihub::plugins {
namespace {
constexpr const char* rootTag = "PLUGINCACHE";
constexpr const char* entryTag = "FILE";

juce::String makeFingerprintAttribute(juce::int64 value) { return juce::String(value); }
}  // namespace

juce::String PluginCache::outcomeToString(Outcome outcome) {
    switch (outcome) {
        case Outcome::success:
            return "success";
        case Outcome::crashed:
            return "crashed";
        case Outcome::timedOut:
            return "timedOut";
        case Outcome::failed:
            return "failed";
    }

    return "failed";
}

PluginCache::Outcome PluginCache::outcomeFromString(const juce::String& text) {
    if (text == "success") return Outcome::success;

    if (text == "crashed") return Outcome::crashed;

    if (text == "timedOut") return Outcome::timedOut;

    return Outcome::failed;
}

bool PluginCache::load() {
    entries.clear();

    if (cacheFile == juce::File() || !cacheFile.existsAsFile()) return false;

    const auto xml = juce::XmlDocument::parse(cacheFile);

    if (xml == nullptr || !xml->hasTagName(rootTag)) return false;

    if (xml->getIntAttribute("version", 0) != formatVersion) return false;

    for (auto* fileElement : xml->getChildIterator()) {
        if (fileElement == nullptr || !fileElement->hasTagName(entryTag)) continue;

        const auto path = fileElement->getStringAttribute("path");

        if (path.isEmpty()) continue;

        Entry entry;
        entry.file = juce::File(path);
        entry.fingerprintSize = fileElement->getStringAttribute("size").getLargeIntValue();
        entry.fingerprintModifiedMs =
            fileElement->getStringAttribute("modified").getLargeIntValue();
        entry.outcome = outcomeFromString(fileElement->getStringAttribute("outcome", "failed"));
        entry.reason = fileElement->getStringAttribute("reason");
        entry.lastAttemptMs = fileElement->getStringAttribute("attempt").getLargeIntValue();

        for (auto* pluginElement : fileElement->getChildIterator()) {
            if (pluginElement == nullptr) continue;

            juce::PluginDescription description;

            if (description.loadFromXml(*pluginElement))
                entry.plugins.push_back(std::move(description));
        }

        if (entry.outcome == Outcome::success && entry.plugins.empty())
            entry.outcome = Outcome::failed;

        entries.push_back(std::move(entry));
    }

    return true;
}

bool PluginCache::save() const {
    if (cacheFile == juce::File()) return false;

    if (!cacheFile.getParentDirectory().createDirectory()) return false;

    juce::XmlElement xml(rootTag);
    xml.setAttribute("version", formatVersion);

    for (const auto& entry : entries) {
        auto* fileElement = xml.createNewChildElement(entryTag);
        fileElement->setAttribute("path", entry.file.getFullPathName());
        fileElement->setAttribute("size", makeFingerprintAttribute(entry.fingerprintSize));
        fileElement->setAttribute("modified",
                                  makeFingerprintAttribute(entry.fingerprintModifiedMs));
        fileElement->setAttribute("outcome", outcomeToString(entry.outcome));
        fileElement->setAttribute("attempt", makeFingerprintAttribute(entry.lastAttemptMs));

        if (entry.reason.isNotEmpty()) fileElement->setAttribute("reason", entry.reason);

        for (const auto& description : entry.plugins)
            if (auto pluginElement = description.createXml())
                fileElement->addChildElement(pluginElement.release());
    }

    return xml.writeTo(cacheFile);
}

}  // namespace ihub::plugins
