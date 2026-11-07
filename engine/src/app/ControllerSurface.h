#pragma once

#include <juce_core/juce_core.h>

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

    void start (Api::StateStore* state,
                Api::MidiBridge* midiBridge,
                const juce::File& projectFile);

    void loadProjectFile (const juce::File& projectFile);

    void refreshLeds();

    void resetMapping();

    void shutdown();

private:
    static constexpr int categoryPreviousButton = 0;
    static constexpr int categoryNextButton = 1;
    static constexpr int presetPreviousButton = 2;
    static constexpr int presetNextButton = 3;

    void createMapping();
    bool handleBrowseButton (int cc, bool pressed);

    std::unique_ptr<Controller::Mapping> mapping;
    std::unique_ptr<Controller::LaunchpadDriver> launchpad;
    Api::StateStore* apiState = nullptr;
    Api::MidiBridge* midiBridge = nullptr;
};

}
