#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <vector>

namespace AudioEngine
{

struct DeviceInfo
{
    juce::String typeName;
    juce::String name;

    bool hasInput = false;
    bool hasOutput = false;
    bool isDefaultInput = false;
    bool isDefaultOutput = false;

    juce::StringArray inputChannelNames;
    juce::StringArray outputChannelNames;

    juce::BigInteger inputChannelMask;
    juce::BigInteger outputChannelMask;

    juce::Array<double> sampleRates;
    juce::Array<int> bufferSizes;
};

struct DeviceTypeInfo
{
    juce::String name;
    juce::StringArray inputDeviceNames;
    juce::StringArray outputDeviceNames;
};

struct DeviceSetup
{
    juce::String deviceTypeName;
    juce::String inputDeviceName;
    juce::String outputDeviceName;

    double sampleRate = 48000.0;
    int bufferSize = 128;

    int numInputChannels = 0;
    int numOutputChannels = 2;

    juce::BigInteger inputChannels;
    juce::BigInteger outputChannels;

    static juce::BigInteger channelMask (int numChannels);
};

class DeviceManager
{
public:
    DeviceManager();
    ~DeviceManager();

    juce::StringArray getDeviceTypeNames();

    std::vector<DeviceTypeInfo> getDeviceTypes();

    std::vector<DeviceInfo> getDevices();

    juce::Array<double> getAvailableSampleRates (const juce::String& typeName,
                                                 const juce::String& deviceName);

    juce::Array<int> getAvailableBufferSizes (const juce::String& typeName,
                                              const juce::String& deviceName);

    juce::String describeDevices();

    bool open (const DeviceSetup& setup, juce::String* errorMessage = nullptr);

    bool setDevice (const DeviceSetup& setup, juce::String* errorMessage = nullptr);

    bool setBufferSize (int bufferSize, juce::String* errorMessage = nullptr);
    bool setSampleRate (double sampleRate, juce::String* errorMessage = nullptr);

    void close();

    bool isOpen() const;
    DeviceSetup getCurrentSetup() const;

    juce::String getCurrentDeviceTypeName() const;
    juce::String getCurrentDeviceName() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    int getCurrentBitDepth() const;

    juce::BigInteger getActiveInputChannels() const;
    juce::BigInteger getActiveOutputChannels() const;

    juce::BigInteger getAvailableInputChannelMask() const;
    juce::BigInteger getAvailableOutputChannelMask() const;

    int getInputLatencyInSamples() const;
    int getOutputLatencyInSamples() const;
    int getXRunCount() const;

    void setAudioCallback (juce::AudioIODeviceCallback* callback);
    juce::AudioIODeviceCallback* getAudioCallback() const noexcept;

    juce::AudioDeviceManager& getNativeDeviceManager() noexcept;
    const juce::AudioDeviceManager& getNativeDeviceManager() const noexcept;

private:
    void ensureInitialised();
    juce::AudioIODeviceType* findDeviceType (const juce::String& typeName);
    std::unique_ptr<juce::AudioIODevice> createDevice (const juce::String& typeName,
                                                       const juce::String& deviceName);
    juce::AudioDeviceManager::AudioDeviceSetup makeNativeSetup (const DeviceSetup& setup) const;
    bool applyNativeSetup (const juce::AudioDeviceManager::AudioDeviceSetup& native,
                           juce::String* errorMessage);

    juce::AudioDeviceManager deviceManager;
    juce::AudioIODeviceCallback* audioCallback = nullptr;
    bool initialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceManager)
};

}
