#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace MappingTest
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr float kFadeMilliseconds = 3.0f;
constexpr int kPreRollBlocks = 60;
constexpr double kHotReloadBudgetMs = 500.0;
constexpr double kDropoutRmsFloor = 0.05;
constexpr double kDeltaThreshold = 0.05;

double nowMs();

void writeFileAtomic (const std::filesystem::path& path, const std::string& contents);

std::string makeSetYaml (const std::string& padInstrument,
                         const std::string& encoderMacro = "cutoff");

}
