#include "TestSupport.h"

namespace SettingsTest
{

void runDefaultChecks()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = settingsFile (directory);

    ihub::settings::SettingsStore store (file);

    check (store.load() == ihub::settings::LoadStatus::missing,
           "a missing settings file reports missing");
    check (! file.existsAsFile(),
           "loading a missing settings file does not create it");

    const auto& values = store.settings();
    check (values.audio.deviceType.isEmpty() && values.audio.deviceName.isEmpty(),
           "audio device defaults are empty");
    check (values.audio.sampleRate == 0 && values.audio.bufferSize == 0,
           "audio format defaults are unset");
    check (nearlyEqual (values.mixer.masterGain, 0.8),
           "master gain defaults to 0.8");
    check (nearlyEqual (values.mixer.inputGain, 1.0),
           "input gain defaults to 1.0");
    check (values.mixer.midiChannel == 1,
           "MIDI channel defaults to 1");
    check (! values.mixer.metronome,
           "metronome defaults to off");
    check (values.midi.enabledInputs.empty(),
           "no MIDI inputs are enabled by default");
    check (! values.account.lanEnabled && values.account.pairingToken.isEmpty(),
           "account defaults are off and tokenless");

    writeTextFile (file, R"({ "version": 1 })");

    ihub::settings::SettingsStore bare (file);

    check (bare.load() == ihub::settings::LoadStatus::loaded,
           "a version-only settings file loads");
    check (nearlyEqual (bare.settings().mixer.masterGain, 0.8)
               && bare.settings().midi.enabledInputs.empty(),
           "omitted sections keep their defaults");

    const auto defaultPath = ihub::settings::SettingsStore::defaultFile();

    check (defaultPath.getFileName() == "settings.json",
           "the default settings file is settings.json");
    check (defaultPath.getParentDirectory().getFileName() == "Instrument Hub",
           "the settings file lives in the Instrument Hub folder");
    check (defaultPath.getParentDirectory().getParentDirectory()
               == juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory),
           "the Instrument Hub folder lives under the user application data directory");

    directory.deleteRecursively();
}

}
