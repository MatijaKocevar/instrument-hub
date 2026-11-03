#pragma once

#include "audio/DeviceManager.h"
#include "audio/settings/AudioDeviceRestore.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace ApiTest::AudioSettings
{

AudioEngine::DeviceInfo makeDevice (const juce::String& type,
                                    const juce::String& name,
                                    bool isDefault = false);

struct RecordingOpener
{
    std::vector<AudioEngine::AudioDevicePreference> calls;
    juce::String failName;

    bool operator() (const AudioEngine::AudioDevicePreference& preference,
                     juce::String& error)
    {
        calls.push_back (preference);

        if (preference.deviceName == failName)
        {
            error = "the device is in use by another process";
            return false;
        }

        return true;
    }
};

struct ManagerOpener
{
    AudioEngine::DeviceManager& manager;

    bool operator() (const AudioEngine::AudioDevicePreference& preference,
                     juce::String& error)
    {
        AudioEngine::DeviceSetup setup;
        setup.deviceTypeName = preference.deviceType;
        setup.outputDeviceName = preference.deviceName;
        setup.sampleRate = preference.sampleRate;
        setup.bufferSize = preference.bufferSize;
        setup.numOutputChannels = 2;
        setup.outputChannels = AudioEngine::DeviceSetup::channelMask (2);
        return manager.open (setup, &error);
    }
};

AudioEngine::AudioDeviceOpener asOpener (RecordingOpener& opener);

const AudioEngine::DeviceInfo* findLoopback (const std::vector<AudioEngine::DeviceInfo>& devices);

juce::File makeTemporaryDirectory();

juce::File temporarySettingsFile (const juce::File& directory);

void runStoredDeviceRestart();
void runMissingStoredFallback();
void runFallbackOrder();
void runUnopenableStoredFallback();
void runCliPrecedence();
void runCliTypePrecedence();
void runCliTypeDropsStoredName();
void runCliFormatPrecedence();
void runCliFormatOnFallback();
void runCliDeviceDropsStoredType();
void runCliFailureDoesNotFallBack();
void runBuiltInDefault();

}
