#include "Profiles.h"

#include "../TestSupport.h"
#include "RestoreSupport.h"

#include "settings/SettingsStore.h"

#include <cmath>
#include <vector>

namespace ApiTest::AudioSettings
{

void runProfileRestore()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("JACK", "Rubix22 Pro"),
        makeDevice ("ALSA", "Rubix22")
    };

    ihub::settings::AudioSettings stored;
    stored.deviceType = "JACK";
    stored.deviceName = "Rubix22 Pro";
    stored.sampleRate = 48000;
    stored.bufferSize = 128;
    stored.profiles.push_back ({
        "rubix22", "JACK", "Rubix22 Pro", 44100, 96, { 0, 1 }, { 0, 1 },
        "pro-audio", 7.29, 7.29, 0
    });

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice (stored,
                                                          {},
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "the profile restore opens the stored device");
    check (std::abs (outcome.preference.sampleRate - 44100.0) < 0.5,
           "the stored soundcard profile overrides the generic stored rate");
    check (outcome.preference.bufferSize == 96,
           "the stored soundcard profile overrides the generic stored buffer");

    RecordingOpener overrideOpener;
    AudioEngine::AudioDeviceOverrides overrides;
    overrides.sampleRate = 48000.0;
    overrides.bufferSize = 128;

    const auto overridden = AudioEngine::restoreAudioDevice (stored,
                                                             overrides,
                                                             devices,
                                                             asOpener (overrideOpener));

    check (overridden.opened
               && std::abs (overridden.preference.sampleRate - 48000.0) < 0.5
               && overridden.preference.bufferSize == 128,
           "explicit command-line overrides still win over the profile");

    ihub::settings::AudioSettings noProfile = stored;
    noProfile.profiles.clear();

    RecordingOpener plainOpener;
    const auto plain = AudioEngine::restoreAudioDevice (noProfile,
                                                        {},
                                                        devices,
                                                        asOpener (plainOpener));

    check (plain.opened
               && std::abs (plain.preference.sampleRate - 48000.0) < 0.5
               && plain.preference.bufferSize == 128,
           "without a profile the generic stored settings still restore");
}

}
