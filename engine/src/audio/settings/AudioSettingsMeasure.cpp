#include "audio/AudioSettings.h"

namespace AudioEngine
{

//==============================================================================
Api::AudioMeasurementState AudioSettingsBridge::measure()
{
    const std::lock_guard<std::mutex> lock (mutex);

    Api::AudioMeasurementState result;

    LatencyProbeOptions options;
    options.sampleRate = manager.isOpen() ? manager.getCurrentSampleRate()
                                          : pendingSampleRate;
    options.bufferSize = manager.isOpen() ? manager.getCurrentBufferSize()
                                          : pendingBufferSize;

    if (! (options.sampleRate > 0.0))
        options.sampleRate = 48000.0;

    if (options.bufferSize <= 0)
        options.bufferSize = 128;

    LatencyProbe probe;
    LatencyResult measured;
    const bool wasOpen = manager.isOpen();

    // Subdevice 0 normally belongs to the running stream; 1..3 are free on the
    // ALSA Loopback card, so the probe can run without stopping audio.
    if (wasOpen)
    {
        for (int subdevice = 1; subdevice <= 3 && ! measured.valid; ++subdevice)
        {
            options.subdevice = subdevice;
            measured = probe.measure (options);
        }
    }

    if (! measured.valid)
    {
        const auto savedSetup = wasOpen ? manager.getCurrentSetup() : DeviceSetup {};

        if (wasOpen)
            manager.close();

        for (int subdevice = 0; subdevice < 4 && ! measured.valid; ++subdevice)
        {
            options.subdevice = subdevice;
            measured = probe.measure (options);
        }

        if (wasOpen)
        {
            juce::String reopenError;

            if (! manager.open (savedSetup, &reopenError) && measured.valid)
            {
                measured.error = measured.error.isNotEmpty()
                                     ? measured.error + "; could not reopen the device: "
                                           + reopenError
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

} // namespace AudioEngine
