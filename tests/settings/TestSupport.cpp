#include "TestSupport.h"

#include <cmath>
#include <iostream>

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

namespace SettingsTest
{

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool nearlyEqual (double first, double second)
{
    return std::abs (first - second) < 1.0e-9;
}

juce::File makeTemporaryDirectory()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-settings-test-"
                                              + juce::Uuid().toString());

    directory.createDirectory();
    return directory;
}

juce::File settingsFile (const juce::File& directory)
{
    return directory.getChildFile ("settings.json");
}

void writeTextFile (const juce::File& file, const juce::String& text)
{
    file.getParentDirectory().createDirectory();
    (void) file.replaceWithText (text);
}

juce::String readTextFile (const juce::File& file)
{
    return file.loadFileAsString();
}

juce::var readJsonFile (const juce::File& file)
{
    juce::var parsed;
    const auto result = juce::JSON::parse (readTextFile (file), parsed);

    check (result.wasOk(), "the settings file is valid JSON: "
                               + result.getErrorMessage().toStdString());

    return parsed;
}

juce::var property (const juce::var& object, const juce::String& name)
{
    if (const auto* dynamicObject = object.getDynamicObject())
        return dynamicObject->getProperty (name);

    return {};
}

bool isOwnerOnly (const juce::File& file)
{
   #if JUCE_WINDOWS
    juce::ignoreUnused (file);
    return true;
   #else
    struct stat info = {};

    if (::stat (file.getFullPathName().toRawUTF8(), &info) != 0)
        return false;

    return (info.st_mode & 0777) == 0600;
   #endif
}

}
