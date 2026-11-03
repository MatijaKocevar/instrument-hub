#include "RestoreSupport.h"

#include "../TestSupport.h"

#include <cmath>

namespace ApiTest::AudioSettings
{

void runCliPrecedence()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.deviceName = juce::String ("Onboard Analog");

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Loopback", 48000, 128 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "the CLI device opens");
    check (outcome.preference.deviceName == "Onboard Analog",
           "the CLI device beats the stored device");
    check (! outcome.usedStoredDevice, "a CLI device is not the stored device");
    check (! outcome.fallback, "a CLI device is not a fallback");
}

void runCliTypePrecedence()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("PipeWire", "System")
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.deviceType = juce::String ("PipeWire");

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Loopback", 48000, 128 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "the CLI type opens a device");
    check (outcome.preference.deviceType == "PipeWire",
           "the CLI type beats the stored type");
    check (outcome.preference.deviceName == "System",
           "the stored name is not carried across a CLI type change");
    check (! outcome.usedStoredDevice, "a CLI type is not the stored device");
}

void runCliTypeDropsStoredName()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.deviceType = juce::String ("ALSA");

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Removed Interface", 48000, 128 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "a CLI type opens even when the stored name is gone");
    check (outcome.preference.deviceName == "Loopback",
           "a CLI type ignores the stored device name");
    check (! outcome.usedStoredDevice, "a CLI type is not the stored device");
}

void runCliFormatPrecedence()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.sampleRate = 96000.0;
    overrides.bufferSize = 256;

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Loopback", 44100, 64 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "the stored device opens with CLI format overrides");
    check (outcome.usedStoredDevice, "the stored device is still reported");
    check (std::abs (outcome.preference.sampleRate - 96000.0) < 0.5,
           "the CLI sample rate beats the stored rate");
    check (outcome.preference.bufferSize == 256,
           "the CLI buffer size beats the stored buffer");
}

void runCliFormatOnFallback()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "PulseAudio Sound Server"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.sampleRate = 96000.0;
    overrides.bufferSize = 256;

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Removed Interface", 44100, 64 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "a CLI format does not stop the stored-device fallback");
    check (outcome.fallback, "the missing stored device is still a fallback");
    check (std::abs (outcome.preference.sampleRate - 96000.0) < 0.5,
           "the CLI sample rate reaches the fallback device");
    check (outcome.preference.bufferSize == 256,
           "the CLI buffer size reaches the fallback device");
}

void runCliDeviceDropsStoredType(){
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("PipeWire", "System")
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.deviceName = juce::String ("System");

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Loopback", 48000, 128 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "a CLI device opens across a stored type change");
    check (outcome.preference.deviceType == "PipeWire",
           "a CLI device is not restricted to the stored type");
    check (outcome.preference.deviceName == "System",
           "the CLI device name selects the device");
    check (! outcome.usedStoredDevice, "a CLI device is not the stored device");
}

void runCliFailureDoesNotFallBack()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    AudioEngine::AudioDeviceOverrides overrides;
    overrides.deviceName = juce::String ("Missing Interface");

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({ "ALSA", "Loopback", 48000, 128 },
                                                          overrides,
                                                          devices,
                                                          asOpener (opener));

    check (! outcome.opened, "an explicit CLI device that is missing does not open");
    check (! outcome.fallback, "an explicit CLI device is never silently replaced");
    check (outcome.reason.contains ("not available"),
           "the CLI failure explains the missing device");
    check (opener.calls.empty(), "no device is opened for a missing CLI name");
}

void runBuiltInDefault()
{
    const std::vector<AudioEngine::DeviceInfo> devices {
        makeDevice ("ALSA", "Loopback"),
        makeDevice ("ALSA", "Onboard Analog", true)
    };

    RecordingOpener opener;
    const auto outcome = AudioEngine::restoreAudioDevice ({},
                                                          {},
                                                          devices,
                                                          asOpener (opener));

    check (outcome.opened, "a first run opens a device");
    check (outcome.preference.deviceName == "Onboard Analog",
           "a first run opens the device default");
    check (! outcome.usedStoredDevice && ! outcome.fallback,
           "a first run is neither a restore nor a fallback");
    check (outcome.reason.isEmpty(), "a clean first run has no reason");
}

}
