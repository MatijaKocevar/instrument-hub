#pragma once

#include <memory>

namespace Api
{
class MidiBridge;
class StateStore;
} // namespace Api

namespace Controller
{
class LaunchpadDriver;
class Mapping;
} // namespace Controller

namespace ihub::app
{

/** Owns the Launchpad driver and its set.yaml mapping.

    Pads arm the slot their binding names; the API state mirrors that selection
    for the UI. The LED refresh also runs from the application's 100 ms poll so
    colour edits made over HTTP reach the hardware.
*/
class ControllerSurface
{
public:
    ControllerSurface();
    ~ControllerSurface();

    /** Starts the mapping watcher and connects to the controller.

        `state` and `midiBridge` may be null when the control server failed to
        start; pad presses then still reach the mapping but cannot publish an
        armed slot.
    */
    void start (Api::StateStore* state, Api::MidiBridge* midiBridge);

    /** Mirrors the `/state` grid onto the Launchpad LEDs. */
    void refreshLeds();

    /** Drops the mapping before the HTTP server stops. */
    void resetMapping();

    /** Disconnects and destroys the driver. */
    void shutdown();

private:
    std::unique_ptr<Controller::Mapping> mapping;
    std::unique_ptr<Controller::LaunchpadDriver> launchpad;
    Api::StateStore* apiState = nullptr;
    Api::MidiBridge* midiBridge = nullptr;
};

} // namespace ihub::app
