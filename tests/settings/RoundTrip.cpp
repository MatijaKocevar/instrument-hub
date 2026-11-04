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
    store.settings().audio.profiles = {
        { "rubix22", "JACK", "Rubix22 Pro", 48000, 96, { 0, 1 }, { 0, 1 },
          "pro-audio", 7.29, 7.29, 0 }
    };
    store.settings().mixer = { 0.65, 1.25, 7, true };
    store.settings().midi.enabledInputs = {
        { "Keystation Mini 32 MK3", "Keystation Mini 32 MK3 MIDI 1" },
        { "Launchpad Mini MK3", "Launchpad Mini MK3 MIDI 1" }
    };

    std::vector<int> learnedNotes;

    for (int note = 40; note < 104; ++note)
        learnedNotes.push_back (note);

    store.settings().midi.gridInputs = {
        { "Nano PAD", "Nano PAD USB MIDI", -1, learnedNotes },
        { "Pad Base", "Pad Base MIDI", 36, {} }
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
    check (loaded.audio.profiles.size() == 1
               && loaded.audio.profiles[0].soundcardKey == "rubix22"
               && loaded.audio.profiles[0].deviceType == "JACK"
               && loaded.audio.profiles[0].deviceName == "Rubix22 Pro"
               && loaded.audio.profiles[0].sampleRate == 48000
               && loaded.audio.profiles[0].bufferSize == 96
               && loaded.audio.profiles[0].inputChannels
                      == std::vector<int> { 0, 1 }
               && loaded.audio.profiles[0].outputChannels
                      == std::vector<int> { 0, 1 }
               && loaded.audio.profiles[0].profile == "pro-audio"
               && nearlyEqual (loaded.audio.profiles[0].latencyMs, 7.29)
               && nearlyEqual (loaded.audio.profiles[0].measuredMs, 7.29)
               && loaded.audio.profiles[0].xruns == 0,
           "the per-soundcard profile round-trips");
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
    check (loaded.midi.gridInputs.size() == 2
               && loaded.midi.gridInputs[0].client == "Nano PAD"
               && loaded.midi.gridInputs[0].port == "Nano PAD USB MIDI"
               && loaded.midi.gridInputs[0].noteBase == -1
               && loaded.midi.gridInputs[0].notes == learnedNotes
               && loaded.midi.gridInputs[1].noteBase == 36
               && loaded.midi.gridInputs[1].notes.empty(),
           "the generic grid roles and learned notes round-trip");
    check (loaded.account.lanEnabled && loaded.account.pairingToken == "pairing-token-123",
           "the account settings round-trip");

    const auto document = readJsonFile (file);

    check (static_cast<int> (property (document, "version"))
               == ihub::settings::schemaVersion,
           "the saved document carries schema version 1");

    directory.deleteRecursively();
}

}
