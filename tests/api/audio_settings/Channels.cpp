#include "Channels.h"

#include "../TestSupport.h"

#include "audio/device/DeviceErrors.h"
#include "audio/settings/AudioSettingsChannels.h"

#include <vector>

namespace ApiTest::AudioSettings
{

namespace
{
bool hasFirstTwo (const juce::BigInteger& mask)
{
    return mask[0] && mask[1] && mask.countNumberOfSetBits() == 2;
}
}

void runChannelDefaults()
{
    const auto outputDefaults = AudioEngine::detail::resolveChannelMap (
        std::nullopt, {}, 0, 2, true);

    check (outputDefaults.count == 2,
           "an unprobed device defaults to two output channels");
    check (hasFirstTwo (outputDefaults.mask),
           "the output default selects the first two channels");

    const auto allInputs = AudioEngine::detail::resolveChannelMap (
        std::nullopt, {}, 8, 8, true);

    check (allInputs.count == 8,
           "a device switch activates every reported input channel");
    check (allInputs.mask.countNumberOfSetBits() == 8,
           "the input default mask matches the reported channel count");

    const auto noInputs = AudioEngine::detail::resolveChannelMap (
        std::nullopt, {}, 0, 0, true);

    check (noInputs.count == 0, "a device without inputs requests no input channels");

    juce::BigInteger current;
    current.setBit (1);

    const auto preserved = AudioEngine::detail::resolveChannelMap (
        std::nullopt, current, 2, 2, false);

    check (preserved.count == 1 && preserved.mask[1] && ! preserved.mask[0],
           "an unchanged device keeps its current channel map");

    const std::vector<int> requested { 0 };
    const auto explicitMap = AudioEngine::detail::resolveChannelMap (
        requested, current, 2, 2, false);

    check (explicitMap.count == 1 && explicitMap.mask[0] && ! explicitMap.mask[1],
           "an explicit channel list replaces the current map");

    check (AudioEngine::detail::describeDeviceError (
               "The device \"hw:CARD=UR22mkII,DEV=0\" is busy (another application is using it).")
               == "the device is in use by another process (PipeWire)",
           "a busy ALSA device reports the PipeWire owner");
    check (AudioEngine::detail::describeDeviceError ("in use by another process")
               == "the device is in use by another process (PipeWire)",
           "an unavailable card reports the PipeWire owner");
    check (AudioEngine::detail::describeDeviceError (
               "The device \"hw:CARD=UR22mkII,DEV=0\" is not available.")
               == "the device is not connected",
           "a missing card reports that it is not connected");
    check (AudioEngine::detail::describeDeviceError (
               "No such device: Steinberg UR22mkII")
               == "the device is not connected",
           "a vanished device reports that it is not connected");
    check (AudioEngine::detail::describeDeviceError (
               "Could not open output device \"hw:CARD=UR22mkII,DEV=0\": "
               "Device or resource busy (-16)")
               == "the device is in use by another process (PipeWire)",
           "a raw ALSA busy error reports the PipeWire owner");
    check (AudioEngine::detail::describeDeviceError ("a format was rejected")
               == "a format was rejected",
           "other device errors pass through unchanged");
}

} // namespace ApiTest::AudioSettings
