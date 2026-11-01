#pragma once

namespace PadAudio
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kChannels = 2;
constexpr int kDefaultSlots = 8;

constexpr int kSettleBlocks = 8;
constexpr int kRmsBlocks = 8;
constexpr int kSwitchBlocks = 32;

constexpr int kFirstPad = 0;   // Keys / Grand Piano -> slot 0
constexpr int kSecondPad = 3;  // Keys / Drawbar Organ -> slot 3

constexpr double kMinimumRmsDbfs = -50.0;
constexpr double kDiscontinuityLimitDbfs = -60.0;
constexpr double kMaximumSwitchDelta = 0.02;
constexpr double kHardSwitchMinimumDelta = 0.05;

constexpr int kLoopbackArmFirstBlock = 12;
constexpr int kLoopbackArmSecondBlock = 60;
constexpr int kLoopbackBlocks = 140;
constexpr int kLoopbackWarmupBlocks = 4;

// T127 page acceptance. Pad 10 is Drums / Acoustic Kit (slot 10); pad 16 is
// Bass / Sub Bass (slot 16). Two different rows prove the idle LED shows the
// patch colour while the armed pad is overridden to red.
constexpr int kPagePad = 10;
constexpr int kPageIdlePad = 16;
constexpr int kPageSettleBlocks = 8;
constexpr int kPageRmsBlocks = 8;

} // namespace PadAudio
