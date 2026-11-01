#pragma once

#include <memory>

namespace Api
{
class MidiBridge;
class StateStore;
}

namespace Controller
{
class LaunchpadDriver;
class Mapping;
}

namespace ihub::app
{

class ControllerSurface
{
public:
    ControllerSurface();
    ~ControllerSurface();

    void start (Api::StateStore* state, Api::MidiBridge* midiBridge);

    void refreshLeds();

    void resetMapping();

    void shutdown();

private:
    std::unique_ptr<Controller::Mapping> mapping;
    std::unique_ptr<Controller::LaunchpadDriver> launchpad;
    Api::StateStore* apiState = nullptr;
    Api::MidiBridge* midiBridge = nullptr;
};

}
