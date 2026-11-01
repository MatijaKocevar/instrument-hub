#include "HardSwitch.h"

#include "Fixtures.h"

namespace AudioTest::SessionSmoke
{

void runHardSwitch()
{
    ihub::Session session;
    juce::String error;

    if (! session.build (makeConfig (0.0f), error))
    {
        check (false, "the hard-switch session builds: " + error.toStdString());
    }
    else
    {
        const auto hard = session.measureSwitch (0, 1);

        std::cout << "      hard switch: discontinuity=" << hard.discontinuityDbfs
                  << " dBFS max_delta=" << hard.maxSampleDeltaDbfs
                  << " dBFS" << std::endl;

        check (hard.rendered, "the hard-switch render produced samples");
        check (hard.discontinuityDbfs > kDiscontinuityLimitDbfs,
               "the discontinuity detector fires on a hard switch");
    }
}

} // namespace AudioTest::SessionSmoke
