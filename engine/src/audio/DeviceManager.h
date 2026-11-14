#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "audio/device/PipeWireQuantum.h"

#include <vector>

namespace AudioEngine
{

    struct DeviceListOptions
    {
        bool raw = false;
    };

    struct DeviceInfo
    {
        juce::String typeName;
        juce::String name;
        juce::String description;
        juce::String juceName;
        juce::String pcmName;

        bool available = true;
        juce::String unavailableReason;

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
        DeviceManager ();
        ~DeviceManager ();

        juce::StringArray getDeviceTypeNames ();

        std::vector<DeviceTypeInfo> getDeviceTypes ();

        void setDeviceListOptions (const DeviceListOptions& options);
        const DeviceListOptions& getDeviceListOptions () const noexcept;
        void setRawDeviceList (bool raw);

        std::vector<DeviceInfo> getDevices ();
        std::vector<DeviceInfo> getRawPcmDevices ();

        juce::Array<double> getAvailableSampleRates (const juce::String& typeName,
                                                     const juce::String& deviceName);

        juce::Array<int> getAvailableBufferSizes (const juce::String& typeName,
                                                  const juce::String& deviceName);

        juce::String describeDevices ();

        bool open (const DeviceSetup& setup, juce::String* errorMessage = nullptr);

        bool setDevice (const DeviceSetup& setup, juce::String* errorMessage = nullptr);

        bool setBufferSize (int bufferSize, juce::String* errorMessage = nullptr);
        bool setSampleRate (double sampleRate, juce::String* errorMessage = nullptr);

        void close ();

        bool isOpen () const;
        DeviceSetup getCurrentSetup () const;

        juce::String getCurrentDeviceTypeName () const;
        juce::String getCurrentDeviceName () const;
        double getCurrentSampleRate () const;
        int getCurrentBufferSize () const;
        int getCurrentBitDepth () const;

        juce::BigInteger getActiveInputChannels () const;
        juce::BigInteger getActiveOutputChannels () const;

        juce::BigInteger getAvailableInputChannelMask () const;
        juce::BigInteger getAvailableOutputChannelMask () const;

        int getInputLatencyInSamples () const;
        int getOutputLatencyInSamples () const;
        int getXRunCount () const;

        void setAudioCallback (juce::AudioIODeviceCallback* callback);
        juce::AudioIODeviceCallback* getAudioCallback () const noexcept;

        juce::AudioDeviceManager& getNativeDeviceManager () noexcept;
        const juce::AudioDeviceManager& getNativeDeviceManager () const noexcept;

    private:
        void ensureInitialised ();
        juce::AudioIODeviceType* findDeviceType (const juce::String& typeName);
        std::unique_ptr<juce::AudioIODevice> createRawDevice (const juce::String& typeName,
                                                              const juce::String& outputName,
                                                              const juce::String& inputName);
        juce::String resolveListedName (const juce::String& typeName,
                                        const juce::String& deviceName);
        std::vector<DeviceInfo> getRawDevices ();
        std::vector<DeviceInfo> getCardDevices ();
        std::vector<DeviceInfo> getSystemDevice ();
        std::vector<DeviceInfo> getTypeDevices ();

        bool probeListedDevice (const juce::String& friendlyName,
                                const juce::String& juceName,
                                const juce::String& pcmName,
                                bool hasInput,
                                DeviceInfo& info,
                                bool& busy);

        juce::AudioDeviceManager::AudioDeviceSetup makeNativeSetup (const DeviceSetup& setup) const;
        bool applyNativeSetup (const juce::AudioDeviceManager::AudioDeviceSetup& native,
                               juce::String* errorMessage);

        juce::AudioDeviceManager deviceManager;
        juce::AudioIODeviceCallback* audioCallback = nullptr;
        bool initialised = false;

        DeviceListOptions listOptions;
        detail::PipeWireQuantum jackQuantum;
        std::vector<DeviceInfo> cardCache;
        bool cardCacheValid = false;
        juce::String currentDisplayName;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceManager)
    };

}
