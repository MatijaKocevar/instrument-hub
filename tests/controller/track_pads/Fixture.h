#pragma once

#include "api/StateStore.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"
#include "controller/launchpad/TrackPadLeds.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace TrackPadTest
{

using Bytes = std::vector<std::uint8_t>;

struct GainChange
{
    int slot = -1;
    float gain = 0.0f;
};

struct Fixture
{
    Controller::Mapping mapping;
    Controller::LaunchpadDriver driver;
    std::unique_ptr<Api::StateStore> state;

    std::vector<GainChange> gains;
    std::vector<std::string> toggledInserts;
    std::vector<Bytes> sent;

    bool load (std::string& errorText);

    void press (int pad);
    void release (int pad);

    Controller::TrackLedSnapshot leds() const;
    void paint();

    int redPads() const;
};

}
