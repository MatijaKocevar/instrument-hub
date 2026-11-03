#pragma once

#include "Rig.h"

#include "controller/LaunchpadLed.h"
#include "controller/LaunchpadProtocol.h"

#include <array>
#include <cstdint>

namespace DriverTest
{

constexpr int captureArmedPad = 42;
constexpr int captureWarmingPad = 7;

struct PadSpec
{
    int colorIndex = 0;
    bool warming = false;
    bool error = false;
};

struct GridSpec
{
    int armedSlot = -1;
    std::array<PadSpec, Launchpad::padCount> pads {};
};

using LedGrid = std::array<Controller::PatchLed, Launchpad::padCount>;

GridSpec makeGrid();

LedGrid paint (Rig& rig, const GridSpec& grid);

int countDifferences (const LedGrid& before, const LedGrid& after, int& changedPad);

bool isFrameFor (const Bytes& bytes,
                 int pad,
                 Launchpad::LedMode mode,
                 std::uint8_t velocity);

}
