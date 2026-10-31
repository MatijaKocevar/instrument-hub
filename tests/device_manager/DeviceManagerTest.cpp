// ihub-device-manager-test: exercises AudioEngine::DeviceManager against the
// ALSA Loopback card documented in plan/TESTING.md.
//
// It opens the Loopback output, switches buffer size 128 -> 256 -> 128 while
// audio is streaming, and hot-swaps to a second Loopback subdevice and back,
// asserting that the audio callback keeps running and nothing crashes.

#include "audio/DeviceManager.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

bool waitFor (const std::function<bool()>& predicate, int timeoutMs = 5000)
{
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs);

    while (! predicate())
    {
        if (juce::Time::getMillisecondCounter() > deadline)
            return false;

        juce::Thread::sleep (5);
    }

    return true;
}

class CountingCallback final : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        lastStartBufferSize = device->getCurrentBufferSizeSamples();
        starts.fetch_add (1, std::memory_order_relaxed);
    }

    void audioDeviceStopped() override
    {
        stops.fetch_add (1, std::memory_order_relaxed);
    }

    void audioDeviceIOCallbackWithContext (const float* const*,
                                           int,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

        lastBlockSize = numSamples;
        blocks.fetch_add (1, std::memory_order_relaxed);
    }

    std::atomic<int> blocks { 0 };
    std::atomic<int> starts { 0 };
    std::atomic<int> stops { 0 };
    std::atomic<int> lastBlockSize { 0 };
    std::atomic<int> lastStartBufferSize { 0 };
};

const AudioEngine::DeviceInfo* findLoopback (const std::vector<AudioEngine::DeviceInfo>& devices,
                                             const AudioEngine::DeviceInfo* exclude = nullptr)
{
    for (const auto& device : devices)
        if (device.hasOutput
            && device.name.containsIgnoreCase ("Loopback")
            && &device != exclude)
            return &device;

    return nullptr;
}

bool contains (const juce::Array<int>& values, int wanted)
{
    return values.contains (wanted);
}

bool contains (const juce::Array<double>& values, double wanted)
{
    for (const auto rate : values)
        if (std::abs (rate - wanted) < 0.5)
            return true;

    return false;
}

bool sameRate (double a, double b)
{
    return std::abs (a - b) < 0.5;
}

void printDevices (const std::vector<AudioEngine::DeviceInfo>& devices)
{
    for (const auto& device : devices)
    {
        std::cout << "device: " << device.typeName << " / " << device.name
                  << " in=" << (device.hasInput ? "yes" : "no")
                  << " out=" << (device.hasOutput ? "yes" : "no")
                  << " rates=" << device.sampleRates.size()
                  << " buffers=" << device.bufferSizes.size()
                  << std::endl;
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AudioEngine::DeviceManager manager;

    const auto typeNames = manager.getDeviceTypeNames();
    std::cout << "device types: " << typeNames.joinIntoString (", ") << std::endl;

    const auto devices = manager.getDevices();
    printDevices (devices);

    const auto* loopback = findLoopback (devices);

    if (loopback == nullptr)
    {
        std::cout << "FAIL: no Loopback output device was enumerated" << std::endl;
        return 1;
    }

    check (contains (loopback->sampleRates, 48000.0),
           "Loopback reports 48000 Hz");
    check (contains (loopback->bufferSizes, 128),
           "Loopback reports a 128-sample buffer");
    check (contains (loopback->bufferSizes, 256),
           "Loopback reports a 256-sample buffer");
    check (loopback->outputChannelNames.size() >= 2,
           "Loopback exposes at least 2 output channels");
    check (loopback->outputChannelMask.countNumberOfSetBits()
               == loopback->outputChannelNames.size(),
           "output channel mask matches the channel count");

    const auto* second = findLoopback (devices, loopback);

    if (second != nullptr)
        std::cout << "second Loopback for hot swap: " << second->name << std::endl;
    else
        std::cout << "note: only one Loopback device; hot swap reopens the same one"
                  << std::endl;

    AudioEngine::DeviceSetup setup;
    setup.deviceTypeName = loopback->typeName;
    setup.outputDeviceName = loopback->name;
    setup.sampleRate = 48000.0;
    setup.bufferSize = 128;
    setup.numOutputChannels = juce::jmin (2, loopback->outputChannelNames.size());

    CountingCallback callback;
    manager.setAudioCallback (&callback);

    juce::String error;

    if (! manager.open (setup, &error))
    {
        std::cout << "FAIL: could not open Loopback: " << error << std::endl;
        return 1;
    }

    check (manager.isOpen(), "device is open");
    check (manager.getCurrentBufferSize() == 128, "opened at 128 samples");
    check (sameRate (manager.getCurrentSampleRate(), 48000.0), "opened at 48000 Hz");
    check (manager.getActiveOutputChannels().countNumberOfSetBits()
               == setup.numOutputChannels,
           "active output mask matches the requested channels");
    check (manager.getAvailableOutputChannelMask().countNumberOfSetBits()
               >= setup.numOutputChannels,
           "available output mask covers the active channels");
    check (manager.getOutputLatencyInSamples() >= 0,
           "output latency is reported");
    check (manager.getInputLatencyInSamples() >= 0,
           "input latency is reported");

    if (! waitFor ([&callback] { return callback.blocks.load() > 0; }))
    {
        std::cout << "FAIL: audio callback never ran" << std::endl;
        return 1;
    }

    check (callback.lastBlockSize.load() > 0, "audio callback received samples");

    // --- 128 -> 256 -------------------------------------------------------
    const auto startsBefore256 = callback.starts.load();
    const auto blocksBefore256 = callback.blocks.load();

    if (! manager.setBufferSize (256, &error))
    {
        std::cout << "FAIL: setBufferSize(256): " << error << std::endl;
        check (false, "switch to 256 samples");
    }
    else
    {
        check (manager.getCurrentBufferSize() == 256, "device is running at 256 samples");
        check (waitFor ([&] { return callback.starts.load() > startsBefore256; }),
               "audio callback restarted for 256 samples");
        check (waitFor ([&] { return callback.blocks.load() > blocksBefore256; }),
               "audio kept streaming after switching to 256 samples");
    }

    // --- 256 -> 128 -------------------------------------------------------
    const auto startsBefore128 = callback.starts.load();
    const auto blocksBefore128 = callback.blocks.load();

    if (! manager.setBufferSize (128, &error))
    {
        std::cout << "FAIL: setBufferSize(128): " << error << std::endl;
        check (false, "switch back to 128 samples");
    }
    else
    {
        check (manager.getCurrentBufferSize() == 128, "device is running at 128 samples");
        check (waitFor ([&] { return callback.starts.load() > startsBefore128; }),
               "audio callback restarted for 128 samples");
        check (waitFor ([&] { return callback.blocks.load() > blocksBefore128; }),
               "audio kept streaming after switching back to 128 samples");
    }

    // --- hot swap ---------------------------------------------------------
    if (second != nullptr)
    {
        auto swapped = setup;
        swapped.outputDeviceName = second->name;

        const auto blocksBeforeSwap = callback.blocks.load();

        if (! manager.setDevice (swapped, &error))
        {
            std::cout << "FAIL: hot swap to " << second->name << ": " << error << std::endl;
            check (false, "hot swap to a second device");
        }
        else
        {
            check (manager.getCurrentDeviceName().containsIgnoreCase ("Loopback"),
                   "hot-swapped device is a Loopback");
            check (waitFor ([&] { return callback.blocks.load() > blocksBeforeSwap; }),
                   "audio kept streaming after the hot swap");
        }

        const auto blocksBeforeBack = callback.blocks.load();

        if (! manager.setDevice (setup, &error))
        {
            std::cout << "FAIL: hot swap back: " << error << std::endl;
            check (false, "hot swap back to the first device");
        }
        else
        {
            check (waitFor ([&] { return callback.blocks.load() > blocksBeforeBack; }),
                   "audio kept streaming after swapping back");
        }
    }

    const auto current = manager.getCurrentSetup();
    check (current.outputDeviceName.isNotEmpty(), "getCurrentSetup reports the device name");
    check (current.bufferSize == manager.getCurrentBufferSize(),
           "getCurrentSetup reports the buffer size");
    check (sameRate (current.sampleRate, manager.getCurrentSampleRate()),
           "getCurrentSetup reports the sample rate");

    const auto description = manager.describeDevices();
    check (description.contains ("Loopback"), "device listing mentions Loopback");
    check (description.contains ("sample rates:"), "device listing prints sample rates");
    check (description.contains ("buffer sizes:"), "device listing prints buffer sizes");

    manager.close();
    manager.setAudioCallback (nullptr);

    check (! manager.isOpen(), "close() releases the device");

    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << ": ihub-device-manager (" << failures << " failure(s))"
              << std::endl;

    return failures == 0 ? 0 : 1;
}
