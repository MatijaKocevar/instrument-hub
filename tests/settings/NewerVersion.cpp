#include "TestSupport.h"

namespace SettingsTest
{

void runNewerVersionChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = settingsFile (directory);

    writeTextFile (file, R"({
        "version": 2,
        "audio": { "deviceType": "ALSA", "deviceName": "Future", "sampleRate": 96000, "bufferSize": 32 },
        "mixer": { "masterGain": 0.25, "inputGain": 1.0, "midiChannel": 3, "metronome": true },
        "futureRoot": { "flag": true }
    })");

    const auto original = readTextFile (file);

    ihub::settings::SettingsStore store (file);

    check (store.load() == ihub::settings::LoadStatus::newerVersion,
           "a newer schema version is rejected");
    check (store.getLastError().isNotEmpty(),
           "the newer schema rejection reports a reason");
    check (file.existsAsFile() && readTextFile (file) == original,
           "loading a newer schema leaves the file untouched");
    check (! directory.getChildFile ("settings.json.bad").exists(),
           "a newer schema file is not quarantined");
    check (nearlyEqual (store.settings().mixer.masterGain, 0.8),
           "a newer schema file is not applied to the defaults");

    store.settings().mixer.masterGain = 0.1;

    check (! store.save(), "saving over a newer schema file is refused");
    check (readTextFile (file) == original,
           "the refused save leaves the newer file byte-identical");
    check (! directory.getChildFile ("settings.json.tmp").exists(),
           "the refused save writes no temporary file");

    directory.deleteRecursively();
}

}
