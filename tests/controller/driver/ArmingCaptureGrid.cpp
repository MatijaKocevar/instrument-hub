#include "ArmingCaptureGrid.h"

namespace DriverTest
{

GridSpec makeGrid()
{
    GridSpec grid;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
        grid.pads[static_cast<std::size_t> (pad)].colorIndex = pad % 16;

    return grid;
}

LedGrid paint (Rig& rig, const GridSpec& grid)
{
    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const auto& spec = grid.pads[static_cast<std::size_t> (pad)];
        const bool armed = pad == grid.armedSlot;

        rig.driver.setPatchLed (pad, spec.colorIndex, armed,
                                armed && spec.warming, spec.error);
    }

    LedGrid leds {};

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
        leds[static_cast<std::size_t> (pad)] = rig.driver.getPadLed (pad);

    return leds;
}

int countDifferences (const LedGrid& before, const LedGrid& after, int& changedPad)
{
    int differences = 0;
    changedPad = -1;

    for (int pad = 0; pad < Launchpad::padCount; ++pad)
    {
        const auto index = static_cast<std::size_t> (pad);

        if (before[index] == after[index])
            continue;

        ++differences;
        changedPad = pad;
    }

    return differences;
}

bool isFrameFor (const Bytes& bytes,
                 int pad,
                 Launchpad::LedMode mode,
                 std::uint8_t velocity)
{
    return bytes.size() == 3
           && bytes[0] == static_cast<std::uint8_t> (
                 0x90u + static_cast<std::uint8_t> (mode))
           && Launchpad::padIndexForNote (bytes[1]) == pad
           && bytes[2] == velocity;
}

}
