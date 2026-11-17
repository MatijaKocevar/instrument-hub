#include "SessionSetup.h"

#include "CommandLine.h"
#include "ProjectSetup.h"

#include "Session.h"
#include "audio/settings/AudioDeviceRestore.h"
#include "settings/SettingsStore.h"

#include <iostream>
#include <utility>

namespace ihub::app {

namespace {
AudioEngine::AudioDeviceOverrides readAudioOverrides(const juce::String& commandLine) {
    AudioEngine::AudioDeviceOverrides overrides;

    if (const auto type = getCommandLineValue(commandLine, "--audio-type"); type.isNotEmpty()) {
        overrides.deviceType = type;
    }

    if (const auto name = getCommandLineValue(commandLine, "--audio-device"); name.isNotEmpty()) {
        overrides.deviceName = name;
    }

    if (const auto rate = getCommandLineValue(commandLine, "--audio-sample-rate");
        rate.isNotEmpty()) {
        overrides.sampleRate = rate.getDoubleValue();
    }

    if (const auto block = getCommandLineValue(commandLine, "--audio-buffer-size");
        block.isNotEmpty()) {
        overrides.bufferSize = block.getIntValue();
    }

    return overrides;
}

void applyProvisionalDevice(ihub::SessionConfig& config,
                            const ihub::settings::AudioSettings& stored,
                            const AudioEngine::AudioDeviceOverrides& overrides) {
    config.device.deviceTypeName = overrides.deviceType.value_or(stored.deviceType);
    config.device.outputDeviceName = overrides.deviceName.value_or(stored.deviceName);
    config.device.sampleRate = overrides.sampleRate.value_or(
        stored.sampleRate > 0 ? static_cast<double>(stored.sampleRate) : 48000.0);
    config.device.bufferSize =
        overrides.bufferSize.value_or(stored.bufferSize > 0 ? stored.bufferSize : 128);
}

AudioEngine::AudioRestoreOutcome restoreDevice(ihub::Session& session,
                                               ihub::settings::SettingsStore* settings,
                                               const juce::String& commandLine) {
    ihub::settings::AudioSettings stored;

    if (settings != nullptr) stored = settings->settings().audio;

    const auto overrides = readAudioOverrides(commandLine);

    const auto opener = [&session](const AudioEngine::AudioDevicePreference& preference,
                                   juce::String& error) {
        ihub::SessionDeviceConfig device;
        device.deviceTypeName = preference.deviceType;
        device.outputDeviceName = preference.deviceName;
        device.sampleRate = preference.sampleRate;
        device.bufferSize = preference.bufferSize;
        device.numOutputChannels = 2;
        device.numInputChannels = 2;
        return session.openDevice(device, &error);
    };

    return AudioEngine::restoreAudioDevice(stored, overrides, session.getDevices().getDevices(),
                                           opener);
}
}  // namespace

SessionStartup startSession(const juce::String& commandLine, const juce::File& projectFile,
                            ihub::settings::SettingsStore* settings) {
    SessionStartup result;

    juce::String projectError;
    bool imported = false;
    const bool projectAvailable = ensureProjectFile(projectFile, projectError, &imported);

    if (!projectAvailable)
        std::cerr << "project: " << projectError << std::endl;
    else if (imported)
        std::cout << "project: imported the bundled set into " << projectFile.getFullPathName()
                  << std::endl;

    ihub::SessionConfig config;
    config.slots = ihub::Session::makeDefaultSet();

    if (projectAvailable) {
        if (!loadProjectSession(projectFile, config, projectError)) {
            std::cerr << "project: " << projectError << std::endl;
            config = ihub::SessionConfig{};
            config.slots = ihub::Session::makeDefaultSet();
        } else {
            std::cout << "project: loaded " << projectFile.getFullPathName() << " ("
                      << config.slots.size() << " slots, " << ihub::toString(config.layout)
                      << " layout)" << std::endl;
        }
    }

    if (commandLine.contains("--no-audio")) {
        std::cout << "session: audio disabled by --no-audio" << std::endl;
        return result;
    }

    ihub::settings::AudioSettings stored;

    if (settings != nullptr) stored = settings->settings().audio;

    const auto overrides = readAudioOverrides(commandLine);

    applyProvisionalDevice(config, stored, overrides);
    config.device.numOutputChannels = 2;
    config.device.numInputChannels = 2;

    auto session = std::make_unique<ihub::Session>();

    juce::String error;

    if (!session->build(config, error)) {
        std::cerr << "session: could not build the set: " << error << std::endl;
        return result;
    }

    const auto outcome = restoreDevice(*session, settings, commandLine);
    result.audioRestore.usedStoredDevice = outcome.usedStoredDevice;
    result.audioRestore.fallback = outcome.fallback;
    result.audioRestore.reason = outcome.reason.toStdString();

    if (!outcome.opened) {
        std::cerr << "session: audio is not running: " << outcome.reason << std::endl;
    } else {
        if (outcome.fallback)
            std::cerr << "session: audio restore: " << outcome.reason << std::endl;

        std::cout << "session: opened " << session->getDevices().getCurrentDeviceName() << " at "
                  << session->getDevices().getCurrentSampleRate() << " Hz / "
                  << session->getDevices().getCurrentBufferSize() << " frames, "
                  << session->getNumSlots() << " slots, project loaded" << std::endl;
    }

    session->armSlot(0);
    result.session = std::move(session);
    return result;
}

}  // namespace ihub::app
