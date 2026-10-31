#pragma once

#include <string>

namespace LaunchkeySim
{

/**
    `ihub-selftest --midi-probe` entry point.

    Creates the virtual Launchkey port, lists it, replays a fixture of pad
    presses through it, verifies that they arrive in order, then writes LED
    messages back to the simulator and verifies they were recorded.

    Returns 0 on success and 1 on any failed check. `fixturePath` is optional;
    an empty string selects the packaged 16-pad fixture.
*/
int runMidiProbe (const std::string& fixturePath = {});

} // namespace LaunchkeySim
