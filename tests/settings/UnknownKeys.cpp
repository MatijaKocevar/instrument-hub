#include "TestSupport.h"

namespace SettingsTest
{

void runUnknownKeyChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = settingsFile (directory);

    writeTextFile (file, R"({
        "version": 1,
        "futureRoot": { "flag": true },
        "audio": {
            "deviceType": "ALSA",
            "deviceName": "Device",
            "sampleRate": 44100,
            "bufferSize": 64,
            "futureAudio": "keep"
        },
        "mixer": {
            "masterGain": 0.5,
            "inputGain": 1.0,
            "midiChannel": 2,
            "metronome": false,
            "futureMixer": [1, 2, 3]
        },
        "midi": {
            "enabledInputs": [ { "client": "Keys", "port": "Keys MIDI 1" } ],
            "futureMidi": 7
        },
        "account": {
            "lanEnabled": true,
            "futureAccount": { "x": 1 }
        }
    })");

    ihub::settings::SettingsStore store (file);

    check (store.load() == ihub::settings::LoadStatus::loaded,
           "a file with unknown keys loads");
    check (nearlyEqual (store.settings().mixer.masterGain, 0.5)
               && store.settings().mixer.midiChannel == 2,
           "the known values are read next to the unknown keys");

    store.settings().mixer.masterGain = 0.75;
    store.settings().audio.deviceName = "Renamed Device";

    check (store.save(), "the rewritten settings save: "
                             + store.getLastError().toStdString());

    const auto document = readJsonFile (file);
    const auto audio = property (document, "audio");
    const auto mixer = property (document, "mixer");
    const auto midi = property (document, "midi");
    const auto account = property (document, "account");

    check (property (property (document, "futureRoot"), "flag").isBool(),
           "an unknown root key survives the rewrite");
    check (property (audio, "futureAudio").toString() == "keep",
           "an unknown key inside audio survives the rewrite");
    check (property (mixer, "futureMixer").isArray()
               && property (mixer, "futureMixer").getArray()->size() == 3,
           "an unknown key inside mixer survives the rewrite");
    check (static_cast<int> (property (midi, "futureMidi")) == 7,
           "an unknown key inside midi survives the rewrite");
    check (property (account, "futureAccount").isObject(),
           "an unknown key inside account survives the rewrite");

    check (nearlyEqual (static_cast<double> (property (mixer, "masterGain")), 0.75),
           "the rewritten file carries the new known value");
    check (property (audio, "deviceName").toString() == "Renamed Device",
           "the rewritten file carries the updated device name");
    check (property (audio, "deviceType").toString() == "ALSA",
           "untouched known values are preserved");

    directory.deleteRecursively();
}

}
