#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <vector>

namespace AudioEngine
{

/** A description of one audio device, as reported by its device type.

    The masks contain one bit per channel that the device exposes, while the
    name arrays carry the per-channel labels JUCE reports (usually
    "channel 1", "channel 2", ...).
*/
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

/** One audio device type (ALSA, JACK, CoreAudio, ...) and its device names. */
struct DeviceTypeInfo
{
    juce::String name;
    juce::StringArray inputDeviceNames;
    juce::StringArray outputDeviceNames;
};

/** Everything needed to open (or hot-swap to) a device.

    The channel masks are explicit because JUCE's "use default channels"
    handling is tied to the channel counts passed to
    `AudioDeviceManager::initialise()`, which this wrapper deliberately calls
    with zero so that constructing a manager never grabs the system default
    device. When a mask is left empty the first `num*Channels` channels are
    used instead.
*/
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

    /** A mask with the first `numChannels` bits set. */
    static juce::BigInteger channelMask (int numChannels);
};

/**
    Thin wrapper around `juce::AudioDeviceManager`.

    It adds device enumeration (types, devices, sample rates, buffer sizes and
    channel masks), latency reporting, and a single `open()`/`setDevice()` entry
    point that can hot-swap the running device without restarting the process.
    The audio callback registered with `setAudioCallback()` survives every
    swap; JUCE stops and restarts it around the new device.

    No method here is called from the audio thread, and none of them take a
    lock that the audio callback needs.
*/
class DeviceManager
{
public:
    DeviceManager();
    ~DeviceManager();

    //==============================================================================
    // Enumeration

    /** Names of the compiled-in device types, e.g. "ALSA". */
    juce::StringArray getDeviceTypeNames();

    /** Every device type with its input and output device names. */
    std::vector<DeviceTypeInfo> getDeviceTypes();

    /** Every device of every type, with rates, buffer sizes and masks.

        Creating the descriptions instantiates (but does not open) each device,
        which is how JUCE exposes the available sample rates.
    */
    std::vector<DeviceInfo> getDevices();

    /** Sample rates a named device reports. Empty when it does not exist. */
    juce::Array<double> getAvailableSampleRates (const juce::String& typeName,
                                                 const juce::String& deviceName);

    /** Buffer sizes a named device reports. Empty when it does not exist. */
    juce::Array<int> getAvailableBufferSizes (const juce::String& typeName,
                                              const juce::String& deviceName);

    /** Human-readable device listing, used by the `--list-devices` flag. */
    juce::String describeDevices();

    //==============================================================================
    // Opening and hot swapping

    /** Opens a device, or swaps to it if another device is already running.

        `errorMessage` receives the JUCE error on failure. Returns true only when
        a device is open afterwards.
    */
    bool open (const DeviceSetup& setup, juce::String* errorMessage = nullptr);

    /** Alias for open(): the running device is replaced without an app restart. */
    bool setDevice (const DeviceSetup& setup, juce::String* errorMessage = nullptr);

    bool setBufferSize (int bufferSize, juce::String* errorMessage = nullptr);
    bool setSampleRate (double sampleRate, juce::String* errorMessage = nullptr);

    /** Stops and releases the current device; the callback stays registered. */
    void close();

    bool isOpen() const;
    DeviceSetup getCurrentSetup() const;

    //==============================================================================
    // Current device state

    juce::String getCurrentDeviceTypeName() const;
    juce::String getCurrentDeviceName() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    int getCurrentBitDepth() const;

    /** Channels currently in use (`getActive*Channels()` on the open device). */
    juce::BigInteger getActiveInputChannels() const;
    juce::BigInteger getActiveOutputChannels() const;

    /** Channels the open device exposes, whether active or not. */
    juce::BigInteger getAvailableInputChannelMask() const;
    juce::BigInteger getAvailableOutputChannelMask() const;

    int getInputLatencyInSamples() const;
    int getOutputLatencyInSamples() const;
    int getXRunCount() const;

    //==============================================================================
    // Audio callback

    /** Registers the callback that every opened device drives. Passing nullptr
        removes the current one. The callback is kept across device swaps.
    */
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

} // namespace AudioEngine
