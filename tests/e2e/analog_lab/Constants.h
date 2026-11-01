#pragma once

namespace AnalogLab
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kSlots = 2;
constexpr int kSkipReturnCode = 77;
constexpr int kNeighbourCount = 1;
constexpr int kArmTimeoutMs = 5000;
constexpr int kWarmTimeoutMs = 20000;
constexpr int kChildTimeoutSeconds = 35;
constexpr int kSettleBlocks = 24;
constexpr int kRenderBlocks = 400;
constexpr int kTailBlocks = 16;
constexpr double kSilenceFloor = 1.0e-4;
constexpr double kMinimumRelativeDifference = 0.05;

}
