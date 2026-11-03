#include "TestSupport.h"

namespace SettingsTest
{

void runCorruptRecoveryChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = settingsFile (directory);
    const auto backup = directory.getChildFile ("settings.json.bad");
    const auto corrupt = juce::String ("{ this is not valid json");

    writeTextFile (file, corrupt);

    ihub::settings::SettingsStore store (file);

    check (store.load() == ihub::settings::LoadStatus::corrupt,
           "a corrupt settings file reports corrupt");
    check (store.getLastError().isNotEmpty(),
           "a corrupt settings load reports a reason");
    check (! file.existsAsFile(),
           "the corrupt settings file is moved out of the way");
    check (backup.existsAsFile(),
           "the corrupt settings file is kept as settings.json.bad");
    check (readTextFile (backup) == corrupt,
           "the .bad file keeps the original bytes");
    check (nearlyEqual (store.settings().mixer.masterGain, 0.8),
           "corrupt recovery continues with defaults");

    store.settings().mixer.masterGain = 0.5;

    check (store.save(), "defaults can be saved after corrupt recovery");
    check (file.existsAsFile(), "the recovered settings file is recreated");
    check (readTextFile (backup) == corrupt,
           "saving does not disturb the .bad backup");

    const auto secondCorrupt = juce::String ("[]");
    writeTextFile (file, secondCorrupt);

    ihub::settings::SettingsStore second (file);

    check (second.load() == ihub::settings::LoadStatus::corrupt,
           "a non-object root is treated as corrupt");
    check (readTextFile (backup) == secondCorrupt,
           "a later corrupt file replaces the previous .bad backup");

    directory.deleteRecursively();
}

}
