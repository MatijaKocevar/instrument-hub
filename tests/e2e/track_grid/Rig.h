#pragma once

#include "track_grid/Leds.h"
#include "track_grid/Memory.h"

#include "Session.h"
#include "api/Server.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <httplib.h>

#include <memory>
#include <string>
#include <vector>

namespace TrackGridE2E
{

class Rig final
{
public:
    Rig();
    ~Rig();

    Rig (const Rig&) = delete;
    Rig& operator= (const Rig&) = delete;

    bool build (std::string& errorText);

    ihub::Session& session() noexcept { return sessionInstance; }
    Api::StateStore& state() noexcept { return *stateInstance; }
    httplib::Client& client() noexcept { return *clientInstance; }
    Controller::LaunchpadDriver& launchpad() noexcept { return driver; }

    const ProcessMemory& beforeWarm() const noexcept { return baselineMemory; }
    const ProcessMemory& afterWarm() const noexcept { return warmMemory; }

    void press (int pad);
    void renderBlocks (int blocks);
    double renderRms (int settleBlocks, int rmsBlocks);
    void paintLeds();

    const std::vector<LedEvent>& ledEvents() const noexcept { return leds; }

private:
    void waitForActiveSandbox();

    juce::File directory;
    juce::File projectFile;

    ProcessMemory baselineMemory;
    ProcessMemory warmMemory;

    ihub::Session sessionInstance;
    std::unique_ptr<Api::StateStore> stateInstance;
    std::unique_ptr<Api::Server> serverInstance;
    std::unique_ptr<httplib::Client> clientInstance;
    Controller::Mapping mapping;
    Controller::LaunchpadDriver driver;

    std::vector<LedEvent> leds;
};

}
