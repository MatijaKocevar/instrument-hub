#pragma once

#include "../AudioMeasurements.h"
#include "../SessionRig.h"

#include <vector>

namespace ApiTest::PatchFxApi
{

constexpr int kSlots = 4;
constexpr int kSettleBlocks = 32;
constexpr int kMeasureBlocks = 64;

struct Context
{
    std::vector<float> baseline;
    double eqFirstRms = 0.0;
};

}
