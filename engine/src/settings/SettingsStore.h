#pragma once

#include "settings/AtomicFileWrite.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace ihub::settings
{

constexpr int schemaVersion = 1;

struct AudioSettings
{
    juce::String deviceType;
    juce::String deviceName;
    int sampleRate = 0;
    int bufferSize = 0;
};

struct MixerSettings
{
    double masterGain = 0.8;
    double inputGain = 1.0;
    int midiChannel = 1;
    bool metronome = false;
};

struct MidiInput
{
    juce::String client;
    juce::String port;
};

struct MidiSettings
{
    std::vector<MidiInput> enabledInputs;
};

struct AccountSettings
{
    bool lanEnabled = false;
    juce::String pairingToken;
};

struct RecentProject
{
    juce::String path;
    juce::String name;
};

struct ProjectSettings
{
    juce::String lastPath;
    std::vector<RecentProject> recent;
};

struct Settings
{
    AudioSettings audio;
    MixerSettings mixer;
    MidiSettings midi;
    AccountSettings account;
    ProjectSettings project;
};

enum class LoadStatus
{
    loaded,
    missing,
    corrupt,
    newerVersion
};

class SettingsStore
{
public:
    static juce::File defaultFile();

    explicit SettingsStore (juce::File file = defaultFile(),
                            AtomicWriter atomicWriter = {});

    LoadStatus load();

    bool save();

    Settings& settings() noexcept { return values; }
    const Settings& settings() const noexcept { return values; }

    LoadStatus getLoadStatus() const noexcept { return status; }
    const juce::String& getLastError() const noexcept { return error; }
    juce::File getFile() const { return file; }

private:
    void moveCorruptFileAside();

    juce::File file;
    AtomicWriter writer;
    Settings values;
    juce::var document;
    LoadStatus status = LoadStatus::missing;
    bool inspected = false;
    juce::String error;
};

}
