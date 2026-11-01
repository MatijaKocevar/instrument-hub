#pragma once

namespace AnalogLab
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 2;
constexpr int kSkipReturnCode = 77;
constexpr int kNeighbourCount = 1; // warm limit 2, exactly the two patches
constexpr int kArmTimeoutMs = 5000;
constexpr int kWarmTimeoutMs = 20000; // Wine/vstbridge startup can exceed 5 s
constexpr int kChildTimeoutSeconds = 35;
constexpr int kSettleBlocks = 24;
constexpr int kRenderBlocks = 400; // ~1.07 s at 48 kHz / 128
constexpr int kTailBlocks = 16;
constexpr double kSilenceFloor = 1.0e-4;
constexpr double kMinimumRelativeDifference = 0.05;

} // namespace AnalogLab
