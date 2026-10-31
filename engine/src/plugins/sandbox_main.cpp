// ihub-sandbox: out-of-process plugin host for one instrument slot (T110).
//
// The engine's SandboxHost creates a shared-memory file and launches this
// helper with its path. The helper loads exactly one plugin, renders blocks
// from the shared request ring into the reply ring, and exits when the host
// says stop or when the host itself disappears.
//
// Because the plugin lives here and not in the engine, a plugin that calls
// abort(), segfaults or hangs can only end this process. The host watchdog
// notices, marks the slot failed, and audio keeps running.
//
// Usage:
//   ihub-sandbox --map <file> --plugin <bundle.vst3> [--param name=value ...]
//   ihub-sandbox --map <file> --fake-tone <hz> [--abort-after <blocks>]
//
// Exit codes: 0 stopped, 2 bad arguments, 3 shared-memory failure, 4 plugin
// load failure, 5 render failure. A plugin crash shows up as a signal instead.

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
    // A crashing plugin is expected traffic here; do not litter the working
    // directory with multi-gigabyte cores.
    struct rlimit limit
    {
        0, 0
    };

    setrlimit (RLIMIT_CORE, &limit);
   #endif
}
} // namespace

int main (int argc, char** argv)
{
    ihub::plugins::capFileDescriptorLimit();
    disableCoreDumps();

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    return ihub::plugins::SandboxClient::run (juce::StringArray (argv, argc));
}
