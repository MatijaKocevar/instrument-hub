#include "SandboxClient.h"
#include "ProcessLimits.h"

#include <juce_events/juce_events.h>

#include <iostream>

#if defined(RLIMIT_CORE)
 #include <sys/resource.h>
#endif

namespace
{
void disableCoreDumps()
{
   #if defined(RLIMIT_CORE)
    struct rlimit limit
    {
        0, 0
    };

    setrlimit (RLIMIT_CORE, &limit);
   #endif
}
}

int main (int argc, char** argv)
{
    ihub::plugins::capFileDescriptorLimit();
    disableCoreDumps();

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    return ihub::plugins::SandboxClient::run (juce::StringArray (argv, argc));
}
