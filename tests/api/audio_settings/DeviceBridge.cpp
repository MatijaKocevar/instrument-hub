#include "DeviceBridge.h"

#include "../JsonSupport.h"
#include "../TestSupport.h"

#include "audio/AudioSettings.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>

namespace ApiTest::AudioSettings
{

void runDeviceBridge()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AudioEngine::AudioSettingsBridge bridge;
    juce::String error;

    if (! bridge.openLoopback (&error))
    {
        std::cout << "FAIL could not open the ALSA Loopback: " << error << std::endl;
        ++failures;
        return;
    }

    auto initial = bridge.snapshot();
    check (initial.open, "the bridge opened a real device");
    check (juce::String (initial.deviceName).containsIgnoreCase ("Loopback"),
           "the bridge opened the Loopback device");
    check (initial.bufferSize == 128, "the bridge opens at 128 frames");
    check (std::abs (initial.sampleRate - 48000.0) < 0.5,
           "the bridge opens at 48 kHz");
    check (initial.outputChannels.size() >= 2
               && initial.outputChannels[0].active
               && initial.outputChannels[1].active,
           "the first two output channels are active");
    check (std::abs (initial.latency.bufferMs - 2.6667) < 0.01,
           "the initial buffer latency is 2.67 ms");

    bridge.setChainLatencySamples (240);
    auto withChain = bridge.snapshot();
    check (std::abs (withChain.latency.chainMs - 5.0) < 0.01,
           "the bridge publishes a 240-sample chain as 5 ms of chain latency");
    check (std::abs (withChain.latency.totalMs
                     - (withChain.latency.bufferMs
                        + withChain.latency.deviceMs
                        + withChain.latency.chainMs)) < 0.01,
           "the total latency includes the chain contribution");
    bridge.setChainLatencySamples (0);

    Api::AudioSettingsPatch bufferPatch;
    bufferPatch.bufferSize = 256;
    std::string patchError;

    check (bridge.apply (bufferPatch, patchError),
           "the bridge applies bufferSize 256");

    auto after = bridge.snapshot();
    check (after.bufferSize == 256, "the device is running at 256 frames");
    check (std::abs (after.latency.bufferMs - 5.3333) < 0.01,
           "the buffer latency readout updates to 5.33 ms");

    bufferPatch.bufferSize = 128;
    check (bridge.apply (bufferPatch, patchError),
           "the bridge returns the buffer to 128 frames");
    check (bridge.snapshot().bufferSize == 128, "the device is back at 128 frames");

    Api::AudioSettingsPatch channelPatch;
    channelPatch.outputChannels = std::vector<int> { 0 };
    check (bridge.apply (channelPatch, patchError),
           "the bridge applies a one-channel output map");

    auto remapped = bridge.snapshot();
    check (remapped.outputChannels[1].active == false,
           "output channel 2 is no longer active");
    check (remapped.outputChannels[0].active, "output channel 1 stays active");

    channelPatch.outputChannels = std::vector<int> { 0, 1 };
    check (bridge.apply (channelPatch, patchError),
           "the bridge restores the stereo output map");

    const auto measurement = bridge.measure();
    check (measurement.valid, "the loopback measurement is valid");
    check (measurement.sampleRate > 0.0 && measurement.bufferSize == 128,
           "the measurement uses the open device rate and buffer");
    check (measurement.roundTripSamples > 0,
           "the measurement reports a positive round trip");

    if (measurement.valid)
    {
        const int expected = measurement.bufferSize * 2;
        const int tolerance = measurement.bufferSize;

        std::cout << "bridge: measured " << measurement.roundTripSamples
                  << " samples (" << measurement.roundTripMs << " ms), expected "
                  << expected << " samples (" << measurement.expectedMs << " ms)"
                  << std::endl;

        check (std::abs (measurement.roundTripSamples - expected) <= tolerance,
               "the measured round trip is within one buffer of 2x buffer");
        check (measurement.roundTripMs > 0.0 && measurement.roundTripMs < 100.0,
               "the measured latency is plausible");
        check (measurement.confidence > 0.0f,
               "the measurement reported a correlation confidence");
    }

    bridge.close();

    AudioEngine::DeviceManager external;
    const auto externalDevices = external.getDevices();
    const AudioEngine::DeviceInfo* loopback = nullptr;

    for (const auto& info : externalDevices)
        if (info.hasOutput && info.name.containsIgnoreCase ("Loopback"))
        {
            loopback = &info;
            break;
        }

    if (loopback == nullptr)
    {
        std::cout << "FAIL no Loopback output was enumerated" << std::endl;
        ++failures;
        return;
    }

    AudioEngine::DeviceSetup externalSetup;
    externalSetup.deviceTypeName = loopback->typeName;
    externalSetup.outputDeviceName = loopback->name;
    externalSetup.sampleRate = 48000.0;
    externalSetup.bufferSize = 256;
    externalSetup.numOutputChannels = juce::jmin (2, loopback->outputChannelNames.size());
    externalSetup.outputChannels = AudioEngine::DeviceSetup::channelMask (
        externalSetup.numOutputChannels);

    for (const auto rate : loopback->sampleRates)
        if (std::abs (rate - 44100.0) < 0.5)
        {
            externalSetup.sampleRate = rate;
            break;
        }

    juce::String externalError;

    if (! external.open (externalSetup, &externalError))
    {
        std::cout << "FAIL could not open an external Loopback manager: "
                  << externalError << std::endl;
        ++failures;
        return;
    }

    AudioEngine::AudioSettingsBridge injected;
    const auto beforeInjection = injected.snapshot();
    check (! beforeInjection.open,
           "a bridge without the session manager reports the device closed");

    injected.setDeviceManager (&external);
    const auto followed = injected.snapshot();

    check (followed.open, "the injected bridge reports the session device open");
    check (followed.deviceName == external.getCurrentDeviceName().toStdString(),
           "the injected bridge reports the session device name");
    check (std::abs (followed.sampleRate - external.getCurrentSampleRate()) < 0.5,
           "the injected bridge reports the session sample rate");
    check (followed.bufferSize == external.getCurrentBufferSize(),
           "the injected bridge reports the session buffer size");

    injected.measure();
    check (external.isOpen(), "measuring leaves the injected session device open");

    external.close();
}

}
