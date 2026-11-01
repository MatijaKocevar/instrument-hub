#include "Switch.h"

#include "Fixtures.h"

namespace AudioTest::SessionSmoke
{

void runSwitch()
{
    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig (3.0f), error))
    {
        check (false, "the default session builds: " + error.toStdString());
    }
    else
    {
        AudioEngine::RtGuard::resetCounters();

        const auto forward = session.measureSwitch (0, 1);
        const auto reverse = session.measureSwitch (1, 0);

        const auto counters = AudioEngine::RtGuard::getCounters();

        checkReport (forward, "slot 0 -> slot 1");
        checkReport (reverse, "slot 1 -> slot 0");

        check (counters.total() == 0,
               "the session render path allocates/locks/logs nothing ("
                   + std::to_string (counters.allocations) + " alloc, "
                   + std::to_string (counters.locks) + " lock, "
                   + std::to_string (counters.logs) + " log)");
        check (AudioEngine::RtGuard::assertNoOpenScopes(),
               "no realtime scope leaked out of the render");

        // The device-facing callback shares the render path; drive one
        // block through it and require the armed slot to be non-silent.
        session.armSlot (1);
        const auto callbackRms = renderCallbackRms (session);
        check (callbackRms > 0.0,
               "the device callback path produces non-zero audio");
    }
}

} // namespace AudioTest::SessionSmoke
