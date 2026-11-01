#pragma once

#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <cstdint>
#include <string>
#include <vector>

namespace DriverTest
{

using Bytes = std::vector<std::uint8_t>;

struct Rig
{
    Controller::Mapping mapping;
    Controller::LaunchpadDriver driver;

    std::vector<Bytes> sent;
    std::vector<int> armedSlots;
    int activePad = -1;

    bool load (std::string& errorText);
};

std::string hex (const Bytes& bytes);
std::string hex (const std::vector<Bytes>& messages);
std::string velocityHex (std::uint8_t value);

Bytes padLed (int pad, Controller::PadState state);
Bytes buttonLed (int cc, Controller::PadState state);

}
