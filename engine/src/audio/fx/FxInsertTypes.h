#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AudioEngine
{

enum class FxInsertType
{
    equalizer = 0,
    compressor,
    delay,
    reverb,
    vst3
};

const char* toString (FxInsertType type) noexcept;

bool fxInsertTypeFromString (std::string_view text, FxInsertType& type) noexcept;

struct FxInsertConfig
{
    FxInsertType type = FxInsertType::equalizer;

    std::string path;

    std::string id;

    std::string name;

    bool bypassed = false;

    float wet = 1.0f;

    std::map<std::string, float> parameters;
};

struct FxInsertPatch
{
    std::optional<bool> bypassed;
    std::optional<float> wet;

    std::optional<int> position;

    std::optional<std::map<std::string, float>> parameters;
};

struct FxInsertSnapshot
{
    int index = 0;
    FxInsertType type = FxInsertType::equalizer;

    std::string kind;

    std::string id;
    std::string name;

    bool bypassed = false;
    float wet = 1.0f;

    int latencySamples = 0;

    std::map<std::string, float> parameters;
};

struct FxChainSnapshot
{
    int slot = -1;

    int instrumentLatencySamples = 0;

    int fxLatencySamples = 0;

    int latencySamples = 0;

    double latencyMs = 0.0;

    std::vector<FxInsertSnapshot> inserts;
};

struct FxTransport
{
    bool playing = false;
    double bpm = 120.0;
    double ppq = 0.0;
};

}
