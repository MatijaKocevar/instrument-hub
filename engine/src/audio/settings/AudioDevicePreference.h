#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <optional>

namespace AudioEngine
{

    struct AudioDevicePreference
    {
        juce::String deviceType;
        juce::String deviceName;
        double sampleRate = 48000.0;
        int bufferSize = 128;
    };

    constexpr double defaultAudioSampleRate = 48000.0;
    constexpr int defaultAudioBufferSize = 128;

    struct AudioDeviceOverrides
    {
        std::optional<juce::String> deviceType;
        std::optional<juce::String> deviceName;
        std::optional<double> sampleRate;
        std::optional<int> bufferSize;
    };

    struct AudioRestoreOutcome
    {
        bool opened = false;
        bool usedStoredDevice = false;
        bool fallback = false;
        AudioDevicePreference preference;
        juce::String reason;
    };

    using AudioDeviceOpener = std::function<bool (const AudioDevicePreference&, juce::String&)>;

}
