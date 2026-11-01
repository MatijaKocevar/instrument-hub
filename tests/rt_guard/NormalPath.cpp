#include "NormalPath.h"
#include "Fixtures.h"

#include "audio/RtGuard.h"

#include <iostream>

namespace RtGuardTest
{

void runNormalPath()
{
    std::cout << "\n-- 200 blocks on the normal path are clean --" << std::endl;

    AudioEngine::RtGuard::setEnabled (true);
    AudioEngine::RtGuard::setStrict (false);
    AudioEngine::RtGuard::resetCounters();

    {
        Harness harness (Injection::none);
        harness.source.setArmed (false);

        harness.render (8);
        const auto warmup = AudioEngine::RtGuard::getCounters();

        AudioEngine::RtGuard::resetCounters();
        AudioEngine::RtGuard::setStrict (true);

        harness.render (200);

        AudioEngine::RtGuard::setStrict (false);
        const auto measured = AudioEngine::RtGuard::getCounters();

        std::cout << "warmup violations=" << warmup.total()
                  << ", measured violations=" << measured.total() << std::endl;

        check (warmup.total() == 0, "no violation during the activation ramp");
        check (measured.total() == 0, "no violation across 200 strict blocks");
        check (AudioEngine::RtGuard::getOpenScopeCount() == 0,
               "every processBlock scope was closed");
        check (AudioEngine::RtGuard::assertNoOpenScopes(),
               "LeakedObjectDetector-style scope check passes");
    }

    check (! AudioEngine::RtGuard::isInsideAudioScope(),
           "the test thread is outside the audio scope again");
}

} // namespace RtGuardTest
