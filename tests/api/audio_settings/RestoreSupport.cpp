#include "RestoreSupport.h"

namespace ApiTest::AudioSettings
{

AudioEngine::DeviceInfo makeDevice (const juce::String& type,
                                    const juce::String& name,
                                    bool isDefault)
{
    AudioEngine::DeviceInfo info;
    info.typeName = type;
    info.name = name;
    info.hasOutput = true;
    info.isDefaultOutput = isDefault;
    info.outputChannelNames.add ("out 1");
    info.outputChannelNames.add ("out 2");
    info.sampleRates = { 44100.0, 48000.0, 96000.0 };
    info.bufferSizes = { 64, 128, 256 };
    return info;
}

AudioEngine::AudioDeviceOpener asOpener (RecordingOpener& opener)
{
    return [&opener] (const AudioEngine::AudioDevicePreference& preference,
                      juce::String& error)
    {
        return opener (preference, error);
    };
}

const AudioEngine::DeviceInfo* findLoopback (const std::vector<AudioEngine::DeviceInfo>& devices)
{
    for (const auto& device : devices)
        if (device.hasOutput && device.name.containsIgnoreCase ("Loopback"))
            return &device;

    return nullptr;
}

juce::File makeTemporaryDirectory()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-audio-restore-test-"
                                              + juce::Uuid().toString());

    directory.createDirectory();
    return directory;
}

juce::File temporarySettingsFile (const juce::File& directory)
{
    return directory.getChildFile ("settings.json");
}

}
