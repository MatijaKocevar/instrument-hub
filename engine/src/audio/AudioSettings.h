#pragma once

#include "DeviceManager.h"
#include "LatencyProbe.h"
#include "TimingMonitor.h"

#include "api/Server.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace ihub::settings
{
class SettingsStore;
}

namespace AudioEngine
{

class AudioSettingsBridge
{
public:
    AudioSettingsBridge();
    ~AudioSettingsBridge() = default;

    void setDeviceManager (DeviceManager* manager);

    void setShowTestDevices (bool show);

    void setSettingsStore (ihub::settings::SettingsStore* store);

    void setRestoreState (const Api::AudioRestoreState& state);

    bool openLoopback (juce::String* errorMessage = nullptr);

    bool openFirstOutput (juce::String* errorMessage = nullptr);

    void close();

    Api::AudioStateSnapshot snapshot();

    bool apply (const Api::AudioSettingsPatch& patch, std::string& error);

    Api::AudioMeasurementState measure();

    void setChainLatencySamples (int samples) noexcept;

    void setTimingMonitor (TimingMonitor* monitor) noexcept;

private:
    struct CachedDevice
    {
        juce::String type;
        juce::String name;
        bool available = true;
        juce::String unavailableReason;
        bool hasInput = false;
        bool hasOutput = false;
        juce::StringArray inputNames;
        juce::StringArray outputNames;
        juce::Array<double> sampleRates;
        juce::Array<int> bufferSizes;
    };

    DeviceManager& liveManager() noexcept
    {
        return injectedManager != nullptr ? *injectedManager : ownedManager;
    }

    const DeviceManager& liveManager() const noexcept
    {
        return injectedManager != nullptr ? *injectedManager : ownedManager;
    }

    void refreshDevices();
    const CachedDevice* findDevice (const juce::String& type,
                                    const juce::String& name) const;
    juce::String firstOutputDeviceOfType (const juce::String& type) const;
    juce::String firstDeviceType() const;

    bool openCachedDevice (const CachedDevice& device,
                           juce::String* errorMessage);

    mutable std::mutex mutex;

    DeviceManager ownedManager;
    DeviceManager probeManager;
    DeviceManager* injectedManager = nullptr;
    std::vector<CachedDevice> devices;
    juce::StringArray deviceTypeNames;

    juce::String pendingType;
    juce::String pendingName;
    double pendingSampleRate = 48000.0;
    int pendingBufferSize = 128;

    ihub::settings::SettingsStore* settingsStore = nullptr;
    Api::AudioRestoreState restoreState;

    std::atomic<int> chainLatencySamples { 0 };
    TimingMonitor* timingMonitor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSettingsBridge)
};

}
