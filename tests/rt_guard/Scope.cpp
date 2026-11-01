#include "Scope.h"
#include "Fixtures.h"

#include "audio/RtGuard.h"

#include <iostream>

namespace RtGuardTest
{

void runScopeBookkeeping()
{
    std::cout << "\n-- scope bookkeeping --" << std::endl;

    check (AudioEngine::RtGuard::getThreadScopeDepth() == 0,
           "scope depth starts at zero");
    check (AudioEngine::RtGuard::getOpenScopeCount() == 0,
           "no scopes are open before the test");

    {
        const AudioEngine::RtGuard::Scope outer;

        check (AudioEngine::RtGuard::isInsideAudioScope(),
               "inside a scope the thread is marked realtime");
        check (AudioEngine::RtGuard::getThreadScopeDepth() == 1,
               "outer scope reports depth 1");
        check (AudioEngine::RtGuard::getOpenScopeCount() == 1,
               "outer scope is counted");

        {
            const AudioEngine::RtGuard::Scope inner;

            check (AudioEngine::RtGuard::getThreadScopeDepth() == 2,
                   "nested scope reports depth 2");
            check (AudioEngine::RtGuard::getOpenScopeCount() == 2,
                   "nested scope is counted");
        }

        check (AudioEngine::RtGuard::getThreadScopeDepth() == 1,
               "depth unwinds to 1");
        check (AudioEngine::RtGuard::getOpenScopeCount() == 1,
               "nested scope was released");
    }

    check (AudioEngine::RtGuard::getThreadScopeDepth() == 0,
           "depth unwinds to 0");
    check (AudioEngine::RtGuard::getOpenScopeCount() == 0,
           "all scopes were released");
    check (! AudioEngine::RtGuard::isInsideAudioScope(),
           "thread is no longer realtime");
}

} // namespace RtGuardTest
