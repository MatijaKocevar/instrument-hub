#include "RestoreSupport.h"

#include "../TestSupport.h"

#include "settings/SettingsStore.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <string>

namespace ApiTest::AudioSettings
{

void runStoredDeviceRestart()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AudioEngine::DeviceManager probe;
    const auto devices = probe.getDevices();
    const auto* loopback = findLoopback (devices);

    check (loopback != nullptr, "the restore test found the ALSA Loopback");

    if (loopback == nullptr)
        return;

    const auto directory = makeTemporaryDirectory();
    const auto file = temporarySettingsFile (directory);

    ihub::settings::SettingsStore writer (file);
    writer.load();
    writer.settings().audio = { loopback->typeName,
                                loopback->name,
                                48000,
                                128 };
    check (writer.save(), "the restore test stores the Loopback preference");
    const auto before = file.loadFileAsString();

    for (int run = 1; run <= 2; ++run)
    {
        ihub::settings::SettingsStore store (file);
        check (store.load() == ihub::settings::LoadStatus::loaded,
               "run " + std::to_string (run) + " loads the settings file");

        AudioEngine::DeviceManager manager;
        ManagerOpener opener { manager };
        const auto outcome = AudioEngine::restoreAudioDevice (store.settings().audio,
                                                              {},
                                                              manager.getDevices(),
                                                              opener);

        check (outcome.opened,
               "run " + std::to_string (run) + " opens the stored device");
        check (outcome.usedStoredDevice,
               "run " + std::to_string (run) + " reports the stored device was used");
        check (! outcome.fallback,
               "run " + std::to_string (run) + " does not fall back");
        check (manager.getCurrentDeviceName() == loopback->name,
               "run " + std::to_string (run) + " opens the stored Loopback");
        check (file.loadFileAsString() == before,
               "run " + std::to_string (run) + " leaves the settings file untouched");

        manager.close();
    }

    directory.deleteRecursively();
}

void runMissingStoredFallback()
{
    const auto directory = makeTemporaryDirectory();
    const auto file = temporarySettingsFile (directory);

    ihub::settings::SettingsStore writer (file);
    writer.load();
    writer.settings().audio = { "ALSA", "Removed Interface 9000", 96000, 256 };

    check (writer.save(), "the missing-device test stores a preference");
    const auto before = file.loadFileAsString();

    ihub::settings::SettingsStore store (file);
    store.load();

    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "PulseAudio Sound Server"),
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice (store.settings().audio,
                                                          {},
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "a stored device that disappeared still opens a fallback");
    check (! outcome.usedStoredDevice,
           "the fallback is not reported as the stored device");
    check (outcome.fallback, "the fallback is reported");
    check (outcome.reason.contains ("not available"),
           "the reason names the missing stored device");
    check (outcome.preference.deviceName == "Onboard Analog",
           "the fallback prefers the device default");
    check (std::abs (outcome.preference.sampleRate - 48000.0) < 0.5
               && outcome.preference.bufferSize == 128,
           "the fallback uses the built-in format");
    check (file.loadFileAsString() == before,
           "a failed restore leaves the stored preference byte-identical");

    directory.deleteRecursively();
}

void runFallbackOrder()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("PipeWire", "System"),
        makeDevice ("ALSA", "PulseAudio Sound Server"),
        makeDevice ("ALSA", "Loopback")
    };

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Gone", 48000, 128 },
                                                          {},
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "the first-ALSA fallback opens");
    check (outcome.preference.deviceName == "PulseAudio Sound Server",
           "without a device default the first ALSA output is used");
}

void runUnopenableStoredFallback()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    RecordingOpener opener;
    opener.failName = "Loopback";

    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Loopback", 48000, 128 },
                                                          {},
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "an unopenable stored device falls back");
    check (outcome.fallback, "the unopenable device is reported as a fallback");
    check (! outcome.usedStoredDevice, "the fallback is not the stored device");
    check (outcome.reason.contains ("could not be opened"),
           "the reason explains the failed open");
    check (outcome.preference.deviceName == "Onboard Analog",
           "the fallback is the device default");
}

}
