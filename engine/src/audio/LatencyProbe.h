#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <vector>

namespace AudioEngine
{

/** One round-trip latency measurement. */
struct LatencyResult
{
    bool valid = false;

    /** Audio samples between the emitted impulse and its capture. */
    int roundTripSamples = 0;

    /** The same value in milliseconds at `sampleRate`. */
    double roundTripMs = 0.0;

    double sampleRate = 0.0;
    int bufferSize = 0;

    /** Correlation peak, normalised to 0..1. */
    float confidence = 0.0f;

    juce::String inputDeviceName;
    juce::String outputDeviceName;
    juce::String error;

    /** Expected value for a loopback at this buffer size: two buffers. */
    double expectedMs() const noexcept
    {
        return sampleRate > 0.0
                   ? static_cast<double> (bufferSize) * 2000.0 / sampleRate
                   : 0.0;
    }
};

/** Settings for a latency probe.

    On Linux the names are raw ALSA PCM names such as
    `hw:CARD=Loopback,DEV=0`. Empty names select the first Loopback card.
*/
struct LatencyProbeOptions
{
    /** Raw ALSA playback PCM, e.g. `hw:CARD=Loopback,DEV=0`. */
    juce::String outputDeviceName;

    /** Raw ALSA capture PCM, e.g. `hw:CARD=Loopback,DEV=1`. */
    juce::String inputDeviceName;

    double sampleRate = 48000.0;
    int bufferSize = 128;

    /** Length of the captured window, in samples. */
    int captureSamples = 8192;

    /** Where the impulse starts in the emitted signal, in samples. */
    int impulseOffset = 512;

    /** ALSA subdevice to use, or -1 for the default (subdevice 0).

        The Loopback card has eight subdevices, so the probe can measure while
        another stream owns subdevice 0.
    */
    int subdevice = -1;
};

/**
    Measures the round-trip latency of an audio device with an impulse.

    The probe opens the requested device full-duplex, writes an impulse into
    the output, records the input in a pre-allocated buffer, then
    cross-correlates capture against the known impulse. Buffers are allocated
    before streaming, so the capture path does no allocation.

    On the ALSA Loopback card this measures the whole playback + capture
    buffer path; the card itself has no physical delay, so the result is a
    lower bound for real hardware.

    `measure()` blocks; call it from a worker or UI thread, never from the
    audio thread.
*/
class LatencyProbe final
{
public:
    LatencyProbe();
    ~LatencyProbe();

    /** Runs one measurement. Retries once if the first capture is unusable. */
    LatencyResult measure (const LatencyProbeOptions& options = {});

    /** Measures the Loopback device at `bufferSize` / `sampleRate`. */
    LatencyResult measure (int bufferSize, double sampleRate = 48000.0);

    /** Cross-correlates `captured` against `reference` and returns the lag
        (in samples) with the largest absolute correlation.

        `maxLag` is the largest lag to test. `peak` receives the normalised
        correlation at that lag (0..1). Pure function, safe to unit test.
    */
    static int findLagSamples (const float* reference,
                               int referenceSamples,
                               const float* captured,
                               int capturedSamples,
                               int maxLag,
                               float* peak = nullptr);

private:
    LatencyResult measureOnce (const LatencyProbeOptions& options);
};

} // namespace AudioEngine
