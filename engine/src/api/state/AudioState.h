#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Api
{

struct AudioChannelState
{
    int index = 0;
    std::string name;
    bool active = false;
};

struct AudioDeviceState
{
    std::string type;
    std::string name;
};

struct AudioLatencyState
{
    double bufferMs = 0.0;
    double deviceMs = 0.0;
    double chainMs = 0.0;
    double totalMs = 0.0;
};

struct AudioRestoreState
{
    bool usedStoredDevice = false;
    bool fallback = false;
    std::string reason;
};

struct AudioStateSnapshot
{
    bool open = false;

    std::string deviceType;
    std::string deviceName;

    std::vector<std::string> deviceTypes;
    std::vector<AudioDeviceState> devices;

    double sampleRate = 0.0;
    int bufferSize = 0;

    std::vector<double> sampleRates;
    std::vector<int> bufferSizes;

    std::vector<AudioChannelState> inputChannels;
    std::vector<AudioChannelState> outputChannels;

    AudioLatencyState latency;

    AudioRestoreState restore;

    std::uint64_t deviceXruns = 0;
    std::uint64_t callbackOverruns = 0;
    std::uint64_t xruns = 0;

    std::uint64_t callbacks = 0;
    double loadPercent = 0.0;
};

struct AudioSettingsPatch
{
    std::optional<std::string> deviceType;
    std::optional<std::string> deviceName;
    std::optional<double> sampleRate;
    std::optional<int> bufferSize;
    std::optional<std::vector<int>> inputChannels;
    std::optional<std::vector<int>> outputChannels;
};

struct AudioMeasurementState
{
    bool valid = false;
    int roundTripSamples = 0;
    double roundTripMs = 0.0;
    double expectedMs = 0.0;
    float confidence = 0.0f;
    double sampleRate = 0.0;
    int bufferSize = 0;
    std::string error;
};

using AudioStateProvider = std::function<AudioStateSnapshot()>;

using AudioApplyHandler = std::function<bool (const AudioSettingsPatch&, std::string&)>;

using AudioMeasureHandler = std::function<AudioMeasurementState()>;

}
