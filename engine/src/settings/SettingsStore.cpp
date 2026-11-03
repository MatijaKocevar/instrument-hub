#include "settings/SettingsStore.h"

#include "settings/SettingsJson.h"

#include <cmath>
#include <utility>

namespace ihub::settings
{
namespace
{

bool readVersion (const juce::var& value, int& version)
{
    if (! value.isInt() && ! value.isInt64() && ! value.isDouble())
        return false;

    const auto number = static_cast<double> (value);

    if (! std::isfinite (number)
        || std::abs (number - std::floor (number)) > 0.0
        || number < 0.0
        || number > 1000000.0)
        return false;

    version = static_cast<int> (number);
    return true;
}

}

juce::File SettingsStore::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Instrument Hub")
        .getChildFile ("settings.json");
}

SettingsStore::SettingsStore (juce::File fileToUse, AtomicWriter atomicWriter)
    : file (std::move (fileToUse)),
      writer (std::move (atomicWriter))
{
}

LoadStatus SettingsStore::load()
{
    values = Settings{};
    document = juce::var();
    error.clear();
    status = LoadStatus::missing;
    inspected = true;

    if (! file.existsAsFile())
        return status;

    juce::var parsed;
    const auto parseResult = juce::JSON::parse (file.loadFileAsString(), parsed);

    if (parseResult.failed() || parsed.getDynamicObject() == nullptr)
    {
        error = parseResult.failed() ? parseResult.getErrorMessage()
                                     : "settings root is not a JSON object";
        moveCorruptFileAside();
        status = LoadStatus::corrupt;
        return status;
    }

    int version = 0;

    if (! readVersion (parsed.getDynamicObject()->getProperty ("version"), version))
    {
        error = "settings version is missing or invalid";
        moveCorruptFileAside();
        status = LoadStatus::corrupt;
        return status;
    }

    if (version > schemaVersion)
    {
        error = "settings version " + juce::String (version)
                + " is newer than " + juce::String (schemaVersion);
        status = LoadStatus::newerVersion;
        return status;
    }

    if (version != schemaVersion)
    {
        error = "settings version " + juce::String (version) + " is not supported";
        moveCorruptFileAside();
        status = LoadStatus::corrupt;
        return status;
    }

    detail::applyDocument (parsed, values);
    document = parsed.clone();
    status = LoadStatus::loaded;
    return status;
}

bool SettingsStore::save()
{
    if (! inspected && file.existsAsFile())
    {
        error = "load() must run before an existing settings file is rewritten";
        return false;
    }

    if (status == LoadStatus::newerVersion)
    {
        error = "settings file has a newer schema version";
        return false;
    }

    const auto next = detail::buildDocument (document, values);
    const auto contents = juce::JSON::toString (next, false);

    if (! writeFileAtomically (file, contents, writer))
    {
        error = "could not write " + file.getFullPathName();
        return false;
    }

    document = next;
    status = LoadStatus::loaded;
    error.clear();
    return true;
}

void SettingsStore::moveCorruptFileAside()
{
    const auto backup = file.getSiblingFile (file.getFileName() + ".bad");

    backup.deleteFile();

    if (! file.moveFileTo (backup))
        error += " (could not move the corrupt file aside)";
}

}
