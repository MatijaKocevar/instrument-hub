#pragma once

#include "Session.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <string>
#include <vector>

namespace PadAudio
{

/** The controller stack exactly as Main.cpp wires it: driver -> mapping -> arm. */
struct PadRig
{
    Controller::Mapping mapping;
    Controller::LaunchpadDriver driver;
    ihub::Session& session;

    std::vector<int> armedRequests;

    explicit PadRig (ihub::Session& sessionToDrive) : session (sessionToDrive) {}

    bool load (const std::string& file, std::string& errorText);

    void press (int pad);
};

} // namespace PadAudio
