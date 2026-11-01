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

/** One (type, device) pair the engine can open. */
struct AudioDeviceState
{
    std::string type;
    std::string name;
};

/** The latency budget shown in the audio settings panel, in milliseconds.

    `bufferMs` is one device buffer, `deviceMs` is the input + output latency
    JUCE reports for the open device, `chainMs` is the summed instrument + FX
    latency of the longest slot chain, and `totalMs` is their sum.
*/
struct AudioLatencyState
{
    double bufferMs = 0.0;
    double deviceMs = 0.0;
    double chainMs = 0.0;
    double totalMs = 0.0;
};

/** Everything `GET /audio` returns. */
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

    /** Device driver xruns. */
    std::uint64_t deviceXruns = 0;
    /** Callback overruns counted by TimingMonitor. */
    std::uint64_t callbackOverruns = 0;
    /** Total xruns (`deviceXruns + callbackOverruns`). */
    std::uint64_t xruns = 0;

    std::uint64_t callbacks = 0;
    double loadPercent = 0.0;
};

/** A partial audio settings update from `POST /audio`. */
struct AudioSettingsPatch
{
    std::optional<std::string> deviceType;
    std::optional<std::string> deviceName;
    std::optional<double> sampleRate;
    std::optional<int> bufferSize;
    std::optional<std::vector<int>> inputChannels;
    std::optional<std::vector<int>> outputChannels;
};

/** The result of `POST /audio/measure`. */
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

/** Applies a settings patch. Returns false with a message on rejection. */
using AudioApplyHandler = std::function<bool (const AudioSettingsPatch&, std::string&)>;

/** Runs the latency probe. Blocks for up to a few seconds. */
using AudioMeasureHandler = std::function<AudioMeasurementState()>;

} // namespace Api
