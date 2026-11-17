#include "audio/settings/AudioDeviceRestore.h"

#include "audio/settings/AudioDeviceSelection.h"
#include "audio/settings/AudioProfiles.h"

namespace AudioEngine {

namespace {
bool openWith(const AudioDevicePreference& preference, const AudioDeviceOpener& opener,
              juce::String& error) {
    error.clear();
    return opener != nullptr && opener(preference, error);
}
}  // namespace

AudioRestoreOutcome restoreAudioDevice(const ihub::settings::AudioSettings& stored,
                                       const AudioDeviceOverrides& overrides,
                                       const std::vector<DeviceInfo>& devices,
                                       const AudioDeviceOpener& opener) {
    AudioRestoreOutcome outcome;

    const bool cliDeviceGiven =
        overrides.deviceType.has_value() || overrides.deviceName.has_value();
    const bool storedDeviceGiven = stored.deviceType.isNotEmpty() || stored.deviceName.isNotEmpty();
    const bool preferStored = storedDeviceGiven && !cliDeviceGiven;

    AudioDevicePreference wanted;
    wanted.deviceType =
        overrides.deviceType.value_or(storedDeviceGiven ? stored.deviceType : juce::String());
    wanted.deviceName =
        overrides.deviceName.value_or(storedDeviceGiven ? stored.deviceName : juce::String());

    if (overrides.deviceType.has_value() && !overrides.deviceName.has_value())
        wanted.deviceName.clear();

    if (overrides.deviceName.has_value() && !overrides.deviceType.has_value())
        wanted.deviceType.clear();

    const auto* profile =
        detail::findAudioProfile(stored.profiles, wanted.deviceType, wanted.deviceName);

    const double storedRate = profile != nullptr && profile->sampleRate > 0
                                  ? static_cast<double>(profile->sampleRate)
                              : stored.sampleRate > 0 ? static_cast<double>(stored.sampleRate)
                                                      : defaultAudioSampleRate;
    const int storedBuffer = profile != nullptr && profile->bufferSize > 0 ? profile->bufferSize
                             : stored.bufferSize > 0                       ? stored.bufferSize
                                                                           : defaultAudioBufferSize;

    wanted.sampleRate =
        overrides.sampleRate.value_or(preferStored ? storedRate : defaultAudioSampleRate);
    wanted.bufferSize =
        overrides.bufferSize.value_or(preferStored ? storedBuffer : defaultAudioBufferSize);

    outcome.preference = wanted;

    const DeviceInfo* attempted = nullptr;
    juce::String failure;

    if (wanted.deviceName.isNotEmpty()) {
        attempted =
            detail::findOutputDevice(devices, wanted.deviceType, wanted.deviceName, preferStored);

        if (attempted != nullptr) {
            const auto preference =
                detail::devicePreference(*attempted, wanted.sampleRate, wanted.bufferSize);

            if (openWith(preference, opener, failure)) {
                outcome.opened = true;
                outcome.usedStoredDevice = preferStored;
                outcome.preference = preference;
                return outcome;
            }

            failure = "device \"" + attempted->name + "\" could not be opened: " + failure;
        } else {
            failure = "device \"" + wanted.deviceName + "\" is not available";
        }
    } else if (wanted.deviceType.isNotEmpty()) {
        attempted = detail::firstOutputDeviceOfType(devices, wanted.deviceType);

        if (attempted != nullptr) {
            const auto preference =
                detail::devicePreference(*attempted, wanted.sampleRate, wanted.bufferSize);

            if (openWith(preference, opener, failure)) {
                outcome.opened = true;
                outcome.usedStoredDevice = preferStored;
                outcome.preference = preference;
                return outcome;
            }

            failure = "device \"" + attempted->name + "\" could not be opened: " + failure;
        } else {
            failure = "no output device of type \"" + wanted.deviceType + "\" is available";
        }
    }

    if (cliDeviceGiven) {
        outcome.reason = failure.isNotEmpty() ? failure : "no audio device was selected";
        return outcome;
    }

    if (storedDeviceGiven && failure.isNotEmpty()) {
        outcome.fallback = true;
        outcome.reason = "stored " + failure;
    }

    AudioDevicePreference fallback;
    juce::String fallbackError;

    const double fallbackRate = overrides.sampleRate.value_or(defaultAudioSampleRate);
    const int fallbackBuffer = overrides.bufferSize.value_or(defaultAudioBufferSize);

    if (detail::openFallbackDevice(devices, attempted, fallbackRate, fallbackBuffer, opener,
                                   fallback, fallbackError)) {
        outcome.opened = true;
        outcome.preference = fallback;
        return outcome;
    }

    if (outcome.reason.isEmpty()) {
        outcome.reason = fallbackError.isNotEmpty()
                             ? "no audio output device could be opened: " + fallbackError
                             : "no audio output device was enumerated";
    } else if (fallbackError.isNotEmpty()) {
        outcome.reason += "; the fallback could not be opened: " + fallbackError;
    }

    return outcome;
}

}  // namespace AudioEngine
