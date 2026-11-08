#pragma once

#include "audio/WarmPool.h"
#include "audio/fx/FxInsertTypes.h"

#include <juce_core/juce_core.h>

#include <string>
#include <vector>

namespace ihub
{

enum class SessionSlotKind
{

    empty,

    initTone,

    sampler,

    plugin,

    audioInput
};

const char* toString (SessionSlotKind kind) noexcept;

enum class SessionWarmState
{
    cold,
    warming,
    warm,
    active,
    fading
};

const char* toString (SessionWarmState state) noexcept;

enum class SessionLayout
{
    tracks
};

const char* toString (SessionLayout layout) noexcept;

struct SessionSlotConfig
{
    std::string name;
    SessionSlotKind kind = SessionSlotKind::empty;

    juce::String path;

    double frequency = 220.0;

    float gain = 1.0f;

    float pan = 0.0f;

    bool muted = false;

    bool solo = false;

    int colorIndex = 0;

    std::string instrumentKind = "empty";
    std::string pluginFormat = "VST3";
    std::string pluginId;
    std::string pluginName;
    std::string pluginManufacturer;
    std::string pluginVersion;
    std::string samplePath;

    juce::MemoryBlock pluginState;

    int inputChannel = 0;

    std::vector<AudioEngine::FxInsertConfig> fxChain;
};

struct SessionTrackConfig
{
    std::string id;
    std::string name;
    int slot = -1;
    std::vector<std::string> hotFx;
};

struct SessionDeviceConfig
{
    juce::String deviceTypeName;
    juce::String outputDeviceName;
    juce::String inputDeviceName;

    double sampleRate = 48000.0;
    int bufferSize = 128;
    int numOutputChannels = 2;
    int numInputChannels = 2;

    bool preferLoopback = false;
};

struct SessionConfig
{
    SessionDeviceConfig device;
    std::vector<SessionSlotConfig> slots;
    std::vector<SessionTrackConfig> tracks;

    SessionLayout layout = SessionLayout::tracks;

    bool sharedPluginHost = false;

    float fadeMilliseconds = 3.0f;

    int initialSlot = 0;

    int neighbourCount = AudioEngine::WarmPool::defaultNeighbourCount;

    std::vector<int> warmSlots;

    int loadDelayMilliseconds = 0;
};

}
