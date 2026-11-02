#pragma once

namespace MultiPatch
{

constexpr int kPatchCount = 64;
constexpr int kBatchSize = 8;
constexpr int kBatchCount = kPatchCount / kBatchSize;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNeighbourCount = 1;
constexpr int kArmTimeoutMs = 5000;
constexpr int kChildTimeoutMs = 20000;

}
