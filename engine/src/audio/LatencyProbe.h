#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <vector>

namespace AudioEngine
{

    struct LatencyResult
    {
        bool valid = false;

        int roundTripSamples = 0;

        double roundTripMs = 0.0;

        double sampleRate = 0.0;
        int bufferSize = 0;

        float confidence = 0.0f;

        juce::String inputDeviceName;
        juce::String outputDeviceName;
        juce::String error;

        double expectedMs () const noexcept
        {
            return sampleRate > 0.0 ? static_cast<double> (bufferSize) * 2000.0 / sampleRate : 0.0;
        }
    };

    struct LatencyProbeOptions
    {
        juce::String outputDeviceName;

        juce::String inputDeviceName;

        double sampleRate = 48000.0;
        int bufferSize = 128;

        int captureSamples = 8192;

        int impulseOffset = 512;

        int subdevice = -1;
    };

    class LatencyProbe final
    {
    public:
        LatencyProbe ();
        ~LatencyProbe ();

        LatencyResult measure (const LatencyProbeOptions& options = {});

        LatencyResult measure (int bufferSize, double sampleRate = 48000.0);

        static int findLagSamples (const float* reference,
                                   int referenceSamples,
                                   const float* captured,
                                   int capturedSamples,
                                   int maxLag,
                                   float* peak = nullptr);

    private:
        LatencyResult measureOnce (const LatencyProbeOptions& options);
    };

}
