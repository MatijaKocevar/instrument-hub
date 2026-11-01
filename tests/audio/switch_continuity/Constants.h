#pragma once

#include <array>
#include <cstdint>

namespace AudioTest::SwitchContinuity
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr int kNumSlots = 4;
constexpr int kSwitchCount = 50;
constexpr float kFadeMilliseconds = 3.0f;

constexpr int kPreRollBlocks = 4;
constexpr int kPreRollSamples = kPreRollBlocks * kBlockSize;
constexpr int kMinimumGapSamples = 16;
constexpr int kMaximumGapSamples = 1600;
constexpr int kTailSamples = 4096;

constexpr std::uint32_t kSeed = 20261031u;

constexpr float kAmplitude = 0.15f;
constexpr std::array<double, kNumSlots> kFrequencies { 180.0, 240.0, 300.0, 360.0 };
constexpr std::array<double, kNumSlots> kInitialPhases { 0.0, 0.7, 1.9, 3.3 };

constexpr double kDeltaThreshold = 0.05;

constexpr double kSpectralCutoffHz = 1000.0;
constexpr int kSpectralFrameSize = 1024;
constexpr double kSpectralThresholdDb = -35.0;
constexpr double kEnergyFloor = 1.0e-30;

} // namespace AudioTest::SwitchContinuity
