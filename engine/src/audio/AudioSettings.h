#pragma once

#include "DeviceManager.h"
#include "LatencyProbe.h"
#include "TimingMonitor.h"

#include "api/Server.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace AudioEngine
{

/**
    The engine side of the audio settings panel (T053).

    Wraps `DeviceManager` with the operations the UI needs: enumerate device
    types and devices, open/hot-swap, change sample rate, buffer size and the
    active channel mapping, publish the latency budget and xrun counters, and
    run the loopback latency probe.

    All methods may be called from HTTP threads. A private mutex serialises
    device operations; it is never touched by the audio callback, which only
    reads atomics from `TimingMonitor`.
*/
class AudioSettingsBridge
{
public:
    AudioSettingsBridge();
    ~AudioSettingsBridge() = default;

    //==============================================================================
    // Device selection

    /** Opens the first ALSA Loopback output (48 kHz / 128), used by `--audio`
        and by the acceptance check.
    */
    bool openLoopback (juce::String* errorMessage = nullptr);

    /** Opens the first available output device. */
    bool openFirstOutput (juce::String* errorMessage = nullptr);

    void close();

    //==============================================================================
    // Control API surface

    /** Current state for `GET /audio`. */
    Api::AudioStateSnapshot snapshot();

    /** Applies `POST /audio`. Returns false with a message when the request is
        invalid or the device rejects the change.
    */
    bool apply (const Api::AudioSettingsPatch& patch, std::string& error);

    /** Runs the latency probe for `POST /audio/measure`.

        While a device is open the probe uses a free ALSA Loopback subdevice so
        the running stream is not interrupted; if every subdevice is busy it
        closes the device for the measurement and reopens it afterwards.
    */
    Api::AudioMeasurementState measure();

    //==============================================================================
    // Readouts fed by the rest of the engine

    /** Longest instrument + FX chain latency, in samples (`T064` fills this). */
    void setChainLatencySamples (int samples) noexcept;

    /** Callback timing source; the bridge only reads its atomics. */
    void setTimingMonitor (TimingMonitor* monitor) noexcept;

private:
    struct CachedDevice
    {
        juce::String type;
        juce::String name;
        bool hasInput = false;
        bool hasOutput = false;
        juce::StringArray inputNames;
        juce::StringArray outputNames;
        juce::Array<double> sampleRates;
        juce::Array<int> bufferSizes;
    };

    void refreshDevices();
    const CachedDevice* findDevice (const juce::String& type,
                                    const juce::String& name) const;
    juce::String firstOutputDeviceOfType (const juce::String& type) const;
    juce::String firstDeviceType() const;

    /** Opens `device` at 48 kHz / 128 frames. The caller holds `mutex`. */
    bool openCachedDevice (const CachedDevice& device,
                           juce::String* errorMessage);

    mutable std::mutex mutex;

    DeviceManager manager;
    std::vector<CachedDevice> devices;

    juce::String pendingType;
    juce::String pendingName;
    double pendingSampleRate = 48000.0;
    int pendingBufferSize = 128;

    std::atomic<int> chainLatencySamples { 0 };
    TimingMonitor* timingMonitor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSettingsBridge)
};

} // namespace AudioEngine
