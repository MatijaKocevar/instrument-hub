#include "Fixtures.h"

#include <algorithm>
#include <string>

namespace ApiTest::AudioSettings
{

void recalculate (Api::AudioStateSnapshot& audio)
{
    const auto rate = audio.sampleRate > 0.0 ? audio.sampleRate : 48000.0;

    audio.latency.bufferMs = static_cast<double> (audio.bufferSize) * 1000.0 / rate;
    audio.latency.totalMs = audio.latency.bufferMs
                            + audio.latency.deviceMs
                            + audio.latency.chainMs;
}

Api::AudioStateSnapshot makeFakeAudio()
{
    Api::AudioStateSnapshot audio;
    audio.open = true;
    audio.deviceType = "ALSA";
    audio.deviceName = "Loopback";
    audio.deviceTypes = { "ALSA" };
    audio.devices = { { "ALSA", "Loopback" } };
    audio.sampleRate = 48000.0;
    audio.bufferSize = 128;
    audio.sampleRates = { 44100.0, 48000.0, 96000.0 };
    audio.bufferSizes = { 64, 128, 256, 512 };

    for (int index = 0; index < 2; ++index)
    {
        audio.inputChannels.push_back ({ index,
                                         "channel " + std::to_string (index + 1),
                                         true });
        audio.outputChannels.push_back ({ index,
                                          "channel " + std::to_string (index + 1),
                                          true });
    }

    audio.latency.deviceMs = 1.5;
    audio.latency.chainMs = 4.0;
    recalculate (audio);

    audio.deviceXruns = 1;
    audio.callbackOverruns = 2;
    audio.xruns = 3;
    audio.callbacks = 4096;
    audio.loadPercent = 6.25;
    return audio;
}

bool FakeBackend::apply (const Api::AudioSettingsPatch& patch, std::string& error)
{
    if (patch.bufferSize.has_value()
        && (*patch.bufferSize < 16 || *patch.bufferSize > 8192))
    {
        error = "bufferSize must be between 16 and 8192";
        return false;
    }

    if (patch.sampleRate.has_value())
        audio.sampleRate = *patch.sampleRate;

    if (patch.bufferSize.has_value())
        audio.bufferSize = *patch.bufferSize;

    if (patch.deviceName.has_value())
        audio.deviceName = *patch.deviceName;

    if (patch.inputChannels.has_value())
    {
        for (auto& channel : audio.inputChannels)
            channel.active = std::find (patch.inputChannels->begin(),
                                        patch.inputChannels->end(),
                                        channel.index) != patch.inputChannels->end();
    }

    if (patch.outputChannels.has_value())
    {
        for (auto& channel : audio.outputChannels)
            channel.active = std::find (patch.outputChannels->begin(),
                                        patch.outputChannels->end(),
                                        channel.index) != patch.outputChannels->end();
    }

    recalculate (audio);
    return true;
}

Api::AudioMeasurementState FakeBackend::measure()
{
    ++measurements;

    Api::AudioMeasurementState result;
    result.valid = true;
    result.roundTripSamples = audio.bufferSize * 2;
    result.roundTripMs = static_cast<double> (result.roundTripSamples)
                         * 1000.0 / audio.sampleRate;
    result.expectedMs = result.roundTripMs;
    result.confidence = 0.95f;
    result.sampleRate = audio.sampleRate;
    result.bufferSize = audio.bufferSize;
    return result;
}

void FakeBackend::attach (Api::Server& server)
{
    server.setAudioHandlers (
        [this] { return audio; },
        [this] (const Api::AudioSettingsPatch& patch, std::string& error)
        {
            return apply (patch, error);
        },
        [this] { return measure(); });
}

}
