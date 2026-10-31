#include "DeviceManager.h"

#include <cmath>

namespace AudioEngine
{

namespace
{
bool sameRate (double a, double b)
{
    return std::abs (a - b) < 0.5;
}

juce::String formatRates (const juce::Array<double>& rates)
{
    juce::String text;

    for (int i = 0; i < rates.size(); ++i)
    {
        if (i > 0)
            text << " ";

        const double rate = rates[i];

        if (std::abs (rate - std::floor (rate)) < 0.001)
            text << static_cast<int> (rate);
        else
            text << juce::String (rate, 2);
    }

    return text;
}

juce::String formatBufferSizes (const juce::Array<int>& sizes)
{
    juce::String text;

    for (int i = 0; i < sizes.size(); ++i)
    {
        if (i > 0)
            text << " ";

        text << sizes[i];
    }

    return text;
}
} // namespace

juce::BigInteger DeviceSetup::channelMask (int numChannels)
{
    juce::BigInteger mask;
    mask.setRange (0, juce::jmax (0, numChannels), true);
    return mask;
}

//==============================================================================
DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager()
{
    if (audioCallback != nullptr)
        deviceManager.removeAudioCallback (audioCallback);
}

void DeviceManager::ensureInitialised()
{
    if (initialised)
        return;

    // Zero channels means no device is selected here; the wrapper opens one
    // only when open() is called, so constructing a manager never takes over
    // the system default output.
    deviceManager.initialise (0, 0, nullptr, false);
    initialised = true;
}

juce::AudioIODeviceType* DeviceManager::findDeviceType (const juce::String& typeName)
{
    ensureInitialised();

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type->getTypeName() == typeName)
        {
            type->scanForDevices();
            return type;
        }
    }

    return nullptr;
}

//==============================================================================
juce::StringArray DeviceManager::getDeviceTypeNames()
{
    ensureInitialised();

    juce::StringArray names;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        names.add (type->getTypeName());
    }

    return names;
}

std::vector<DeviceTypeInfo> DeviceManager::getDeviceTypes()
{
    ensureInitialised();

    std::vector<DeviceTypeInfo> result;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();

        DeviceTypeInfo info;
        info.name = type->getTypeName();
        info.inputDeviceNames = type->getDeviceNames (true);
        info.outputDeviceNames = type->getDeviceNames (false);
        result.push_back (std::move (info));
    }

    return result;
}

std::unique_ptr<juce::AudioIODevice> DeviceManager::createDevice (const juce::String& typeName,
                                                                  const juce::String& deviceName)
{
    auto* type = findDeviceType (typeName);

    if (type == nullptr || deviceName.isEmpty())
        return {};

    const auto inputNames = type->getDeviceNames (true);
    const auto outputNames = type->getDeviceNames (false);

    const bool hasInput = inputNames.contains (deviceName);
    const bool hasOutput = outputNames.contains (deviceName);

    if (! hasInput && ! hasOutput)
        return {};

    // ALSA keeps separate input and output name lists; passing the matching
    // name for both sides lets JUCE report the rates for the combined device.
    return std::unique_ptr<juce::AudioIODevice> (
        type->createDevice (hasOutput ? deviceName : juce::String(),
                            hasInput ? deviceName : juce::String()));
}

std::vector<DeviceInfo> DeviceManager::getDevices()
{
    ensureInitialised();

    std::vector<DeviceInfo> result;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        type->scanForDevices();

        const auto inputNames = type->getDeviceNames (true);
        const auto outputNames = type->getDeviceNames (false);
        const int defaultInput = type->getDefaultDeviceIndex (true);
        const int defaultOutput = type->getDefaultDeviceIndex (false);

        juce::StringArray allNames (inputNames);

        for (const auto& name : outputNames)
            if (! allNames.contains (name))
                allNames.add (name);

        for (const auto& name : allNames)
        {
            auto device = createDevice (type->getTypeName(), name);

            if (device == nullptr)
                continue;

            DeviceInfo info;
            info.typeName = type->getTypeName();
            info.name = name;
            info.hasInput = inputNames.contains (name);
            info.hasOutput = outputNames.contains (name);
            info.isDefaultInput = info.hasInput && defaultInput >= 0
                                  && inputNames[defaultInput] == name;
            info.isDefaultOutput = info.hasOutput && defaultOutput >= 0
                                   && outputNames[defaultOutput] == name;

            info.inputChannelNames = device->getInputChannelNames();
            info.outputChannelNames = device->getOutputChannelNames();
            info.inputChannelMask.setRange (0, info.inputChannelNames.size(), true);
            info.outputChannelMask.setRange (0, info.outputChannelNames.size(), true);

            info.sampleRates = device->getAvailableSampleRates();
            info.bufferSizes = device->getAvailableBufferSizes();

            result.push_back (std::move (info));
        }
    }

    return result;
}

juce::Array<double> DeviceManager::getAvailableSampleRates (const juce::String& typeName,
                                                            const juce::String& deviceName)
{
    if (auto device = createDevice (typeName, deviceName))
        return device->getAvailableSampleRates();

    return {};
}

juce::Array<int> DeviceManager::getAvailableBufferSizes (const juce::String& typeName,
                                                         const juce::String& deviceName)
{
    if (auto device = createDevice (typeName, deviceName))
        return device->getAvailableBufferSizes();

    return {};
}

juce::String DeviceManager::describeDevices()
{
    juce::String text;
    const auto typeNames = getDeviceTypeNames();

    text << "audio device types: "
         << (typeNames.isEmpty() ? juce::String ("none")
                                 : typeNames.joinIntoString (", "))
         << "\n";

    for (const auto& type : getDeviceTypes())
    {
        text << "\ndevice type: " << type.name << "\n"
             << "  inputs : "
             << (type.inputDeviceNames.isEmpty()
                     ? juce::String ("none")
                     : type.inputDeviceNames.joinIntoString (", "))
             << "\n"
             << "  outputs: "
             << (type.outputDeviceNames.isEmpty()
                     ? juce::String ("none")
                     : type.outputDeviceNames.joinIntoString (", "))
             << "\n";
    }

    for (const auto& info : getDevices())
    {
        text << "\ndevice: " << info.name << "\n"
             << "  type   : " << info.typeName << "\n"
             << "  input  : " << (info.hasInput ? "yes" : "no");

        if (info.isDefaultInput)
            text << " (default)";

        text << " channels=" << info.inputChannelNames.size()
             << " mask=" << info.inputChannelMask.toString (2) << "\n"
             << "  output : " << (info.hasOutput ? "yes" : "no");

        if (info.isDefaultOutput)
            text << " (default)";

        text << " channels=" << info.outputChannelNames.size()
             << " mask=" << info.outputChannelMask.toString (2) << "\n"
             << "  sample rates: " << formatRates (info.sampleRates) << "\n"
             << "  buffer sizes: " << formatBufferSizes (info.bufferSizes) << "\n";
    }

    return text;
}

//==============================================================================
juce::AudioDeviceManager::AudioDeviceSetup DeviceManager::makeNativeSetup (const DeviceSetup& setup) const
{
    juce::AudioDeviceManager::AudioDeviceSetup native;
    native.inputDeviceName = setup.inputDeviceName;
    native.outputDeviceName = setup.outputDeviceName;
    native.sampleRate = setup.sampleRate;
    native.bufferSize = setup.bufferSize;
    native.useDefaultInputChannels = false;
    native.useDefaultOutputChannels = false;

    native.inputChannels = setup.inputChannels;

    if (native.inputChannels.isZero() && setup.inputDeviceName.isNotEmpty())
        native.inputChannels = DeviceSetup::channelMask (setup.numInputChannels);

    native.outputChannels = setup.outputChannels;

    if (native.outputChannels.isZero() && setup.outputDeviceName.isNotEmpty())
        native.outputChannels = DeviceSetup::channelMask (setup.numOutputChannels);

    return native;
}

bool DeviceManager::applyNativeSetup (const juce::AudioDeviceManager::AudioDeviceSetup& native,
                                      juce::String* errorMessage)
{
    const auto error = deviceManager.setAudioDeviceSetup (native, true);
    const bool open = deviceManager.getCurrentAudioDevice() != nullptr;

    if (errorMessage != nullptr)
    {
        if (error.isNotEmpty())
            *errorMessage = error;
        else if (! open)
            *errorMessage = "no audio device was opened";
    }

    return error.isEmpty() && open;
}

bool DeviceManager::open (const DeviceSetup& setup, juce::String* errorMessage)
{
    ensureInitialised();

    if (setup.inputDeviceName.isEmpty() && setup.outputDeviceName.isEmpty())
    {
        if (errorMessage != nullptr)
            *errorMessage = "no input or output device name given";

        return false;
    }

    auto typeName = setup.deviceTypeName;

    if (typeName.isEmpty())
        typeName = deviceManager.getCurrentAudioDeviceType();

    if (findDeviceType (typeName) == nullptr)
    {
        const auto available = getDeviceTypeNames();

        if (available.isEmpty())
        {
            if (errorMessage != nullptr)
                *errorMessage = "no audio device types are available";

            return false;
        }

        typeName = available[0];
    }

    if (deviceManager.getCurrentAudioDeviceType() != typeName)
    {
        // This may briefly open the type's default device; the explicit setup
        // below replaces it in the same call, so no redundant streaming.
        deviceManager.setCurrentAudioDeviceType (typeName, true);
    }

    return applyNativeSetup (makeNativeSetup (setup), errorMessage);
}

bool DeviceManager::setDevice (const DeviceSetup& setup, juce::String* errorMessage)
{
    return open (setup, errorMessage);
}

bool DeviceManager::setBufferSize (int bufferSize, juce::String* errorMessage)
{
    if (! isOpen())
    {
        if (errorMessage != nullptr)
            *errorMessage = "no audio device is open";

        return false;
    }

    auto native = deviceManager.getAudioDeviceSetup();

    if (native.bufferSize == bufferSize
        && getCurrentBufferSize() == bufferSize)
        return true;

    native.bufferSize = bufferSize;

    if (! applyNativeSetup (native, errorMessage))
        return false;

    const int actual = getCurrentBufferSize();

    if (actual != bufferSize)
    {
        if (errorMessage != nullptr)
            *errorMessage = "device selected " + juce::String (actual)
                            + " samples instead of " + juce::String (bufferSize);

        return false;
    }

    return true;
}

bool DeviceManager::setSampleRate (double sampleRate, juce::String* errorMessage)
{
    if (! isOpen())
    {
        if (errorMessage != nullptr)
            *errorMessage = "no audio device is open";

        return false;
    }

    auto native = deviceManager.getAudioDeviceSetup();

    if (sameRate (native.sampleRate, sampleRate)
        && sameRate (getCurrentSampleRate(), sampleRate))
        return true;

    native.sampleRate = sampleRate;

    if (! applyNativeSetup (native, errorMessage))
        return false;

    if (! sameRate (getCurrentSampleRate(), sampleRate))
    {
        if (errorMessage != nullptr)
            *errorMessage = "device selected " + juce::String (getCurrentSampleRate())
                            + " Hz instead of " + juce::String (sampleRate) + " Hz";

        return false;
    }

    return true;
}

void DeviceManager::close()
{
    deviceManager.closeAudioDevice();
}

bool DeviceManager::isOpen() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

DeviceSetup DeviceManager::getCurrentSetup() const
{
    DeviceSetup setup;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto native = deviceManager.getAudioDeviceSetup();
        setup.inputDeviceName = native.inputDeviceName;
        setup.outputDeviceName = native.outputDeviceName;
        setup.sampleRate = device->getCurrentSampleRate();
        setup.bufferSize = device->getCurrentBufferSizeSamples();
        setup.inputChannels = device->getActiveInputChannels();
        setup.outputChannels = device->getActiveOutputChannels();
        setup.numInputChannels = setup.inputChannels.countNumberOfSetBits();
        setup.numOutputChannels = setup.outputChannels.countNumberOfSetBits();
    }

    setup.deviceTypeName = getCurrentDeviceTypeName();
    return setup;
}

//==============================================================================
juce::String DeviceManager::getCurrentDeviceTypeName() const
{
    return deviceManager.getCurrentAudioDeviceType();
}

juce::String DeviceManager::getCurrentDeviceName() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getName();

    return {};
}

double DeviceManager::getCurrentSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();

    return 0.0;
}

int DeviceManager::getCurrentBufferSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();

    return 0;
}

int DeviceManager::getCurrentBitDepth() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBitDepth();

    return 0;
}

juce::BigInteger DeviceManager::getActiveInputChannels() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveInputChannels();

    return {};
}

juce::BigInteger DeviceManager::getActiveOutputChannels() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveOutputChannels();

    return {};
}

juce::BigInteger DeviceManager::getAvailableInputChannelMask() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::BigInteger mask;
        mask.setRange (0, device->getInputChannelNames().size(), true);
        return mask;
    }

    return {};
}

juce::BigInteger DeviceManager::getAvailableOutputChannelMask() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::BigInteger mask;
        mask.setRange (0, device->getOutputChannelNames().size(), true);
        return mask;
    }

    return {};
}

int DeviceManager::getInputLatencyInSamples() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getInputLatencyInSamples();

    return 0;
}

int DeviceManager::getOutputLatencyInSamples() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getOutputLatencyInSamples();

    return 0;
}

int DeviceManager::getXRunCount() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getXRunCount();

    return 0;
}

//==============================================================================
void DeviceManager::setAudioCallback (juce::AudioIODeviceCallback* callback)
{
    if (callback == audioCallback)
        return;

    if (audioCallback != nullptr)
        deviceManager.removeAudioCallback (audioCallback);

    audioCallback = callback;

    if (audioCallback != nullptr)
        deviceManager.addAudioCallback (audioCallback);
}

juce::AudioIODeviceCallback* DeviceManager::getAudioCallback() const noexcept
{
    return audioCallback;
}

juce::AudioDeviceManager& DeviceManager::getNativeDeviceManager() noexcept
{
    return deviceManager;
}

const juce::AudioDeviceManager& DeviceManager::getNativeDeviceManager() const noexcept
{
    return deviceManager;
}

} // namespace AudioEngine
