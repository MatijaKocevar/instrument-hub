#pragma once

#include <array>

namespace TrackGridE2E
{

constexpr int kTrackCount = 8;
constexpr int kChannels = 2;
constexpr int kBlockSize = 128;
constexpr double kSampleRate = 48000.0;

constexpr int kSettleBlocks = 4;
constexpr int kRmsBlocks = 8;
constexpr int kSwitchBlocks = 4;

constexpr int kTrackPadBase = 56;
constexpr int kHot0PadBase = 48;
constexpr int kHot1PadBase = 40;
constexpr int kVolumeRowStride = 8;

constexpr int kVolumeBoostStep = 4;
constexpr double kMinimumRmsDbfs = -50.0;
constexpr double kMaximumBlockMs = 1000.0 * kBlockSize / kSampleRate;

constexpr std::array<double, kTrackCount> kFrequencies {
    110.0, 130.81, 146.83, 164.81, 196.0, 220.0, 246.94, 293.66
};

constexpr int trackPad (int track)
{
    return kTrackPadBase + track;
}

constexpr int hotPad (int hot, int track)
{
    return (hot == 0 ? kHot0PadBase : kHot1PadBase) + track;
}

constexpr int volumePad (int step, int track)
{
    return step * kVolumeRowStride + track;
}

}
