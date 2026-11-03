#include "TestSupport.h"

namespace SettingsTest
{

void runRoundTripChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = settingsFile (directory);
    const auto temporary = directory.getChildFile ("settings.json.tmp");

    ihub::settings::SettingsStore store (file);

    check (store.load() == ihub::settings::LoadStatus::missing,
           "the round trip starts from a missing file");

    store.settings().audio = { "ALSA", "Steinberg UR22mkII", 48000, 128 };
    store.settings().mixer = { 0.65, 1.25, 7, true };
    store.settings().midi.enabledInputs = {
        { "Keystation Mini 32 MK3", "Keystation Mini 32 MK3 MIDI 1" },
        { "Launchpad Mini MK3", "Launchpad Mini MK3 MIDI 1" }
    };
    store.settings().account = { true, "pairing-token-123" };

    check (store.save(), "the settings save: " + store.getLastError().toStdString());
    check (file.existsAsFile(), "the settings file is created");
    check (! temporary.exists(),
           "a successful save leaves no temporary file behind");
    check (isOwnerOnly (file), "the settings file is readable by its owner only");

    ihub::settings::SettingsStore reloaded (file);

    check (reloaded.load() == ihub::settings::LoadStatus::loaded,
           "the saved settings file reloads");

    const auto& loaded = reloaded.settings();
    check (loaded.audio.deviceType == "ALSA"
               && loaded.audio.deviceName == "Steinberg UR22mkII"
               && loaded.audio.sampleRate == 48000
               && loaded.audio.bufferSize == 128,
           "the audio device settings round-trip");
    check (nearlyEqual (loaded.mixer.masterGain, 0.65)
               && nearlyEqual (loaded.mixer.inputGain, 1.25)
               && loaded.mixer.midiChannel == 7
               && loaded.mixer.metronome,
           "the mixer settings round-trip");
    check (loaded.midi.enabledInputs.size() == 2
               && loaded.midi.enabledInputs[0].client == "Keystation Mini 32 MK3"
               && loaded.midi.enabledInputs[0].port == "Keystation Mini 32 MK3 MIDI 1"
               && loaded.midi.enabledInputs[1].client == "Launchpad Mini MK3"
               && loaded.midi.enabledInputs[1].port == "Launchpad Mini MK3 MIDI 1",
           "the enabled MIDI inputs round-trip as names");
    check (loaded.account.lanEnabled && loaded.account.pairingToken == "pairing-token-123",
           "the account settings round-trip");

    const auto document = readJsonFile (file);

    check (static_cast<int> (property (document, "version"))
               == ihub::settings::schemaVersion,
           "the saved document carries schema version 1");

    directory.deleteRecursively();
}

}
