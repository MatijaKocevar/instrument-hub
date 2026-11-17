#include "audio/AudioSettings.h"

namespace AudioEngine {

Api::AudioMeasurementState AudioSettingsBridge::measure() {
    const std::lock_guard<std::mutex> lock(mutex);

    auto& live = liveManager();
    const bool liveOpen = live.isOpen();

    Api::AudioMeasurementState result;

    LatencyProbeOptions options;
    options.sampleRate = liveOpen ? live.getCurrentSampleRate() : pendingSampleRate;
    options.bufferSize = liveOpen ? live.getCurrentBufferSize() : pendingBufferSize;

    if (!(options.sampleRate > 0.0)) options.sampleRate = 48000.0;

    if (options.bufferSize <= 0) options.bufferSize = 128;

    LatencyProbe probe;
    LatencyResult measured;

    if (liveOpen) {
        for (int subdevice = 1; subdevice <= 3 && !measured.valid; ++subdevice) {
            options.subdevice = subdevice;
            measured = probe.measure(options);
        }
    }

    if (!measured.valid) {
        auto& probeDevices = injectedManager != nullptr ? probeManager : ownedManager;
        const bool probeWasOpen = probeDevices.isOpen();
        const auto savedSetup = probeWasOpen ? probeDevices.getCurrentSetup() : DeviceSetup{};

        if (probeWasOpen) probeDevices.close();

        for (int subdevice = 0; subdevice < 4 && !measured.valid; ++subdevice) {
            options.subdevice = subdevice;
            measured = probe.measure(options);
        }

        if (probeWasOpen) {
            juce::String reopenError;

            if (!probeDevices.open(savedSetup, &reopenError) && measured.valid) {
                measured.error =
                    measured.error.isNotEmpty()
                        ? measured.error + "; could not reopen the device: " + reopenError
                        : "could not reopen the device: " + reopenError;
            }
        }
    }

    result.valid = measured.valid;
    result.roundTripSamples = measured.roundTripSamples;
    result.roundTripMs = measured.roundTripMs;
    result.expectedMs = measured.expectedMs();
    result.confidence = measured.confidence;
    result.sampleRate = measured.sampleRate;
    result.bufferSize = measured.bufferSize;
    result.error = measured.error.toStdString();
    return result;
}

}  // namespace AudioEngine
