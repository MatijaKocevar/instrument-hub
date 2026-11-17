#include "ProcessLimits.h"
#include "SandboxClient.h"
#include "sandbox/SandboxPageClient.h"

#include <juce_events/juce_events.h>

#include <iostream>

#if defined(RLIMIT_CORE)
#include <sys/resource.h>
#endif

namespace {
void disableCoreDumps() {
#if defined(RLIMIT_CORE)
  struct rlimit limit{0, 0};

  setrlimit(RLIMIT_CORE, &limit);
#endif
}
}  // namespace

int main(int argc, char** argv) {
  ihub::plugins::capFileDescriptorLimit();
  disableCoreDumps();

  juce::ScopedJuceInitialiser_GUI juceInitialiser;

  const juce::StringArray commandLine(argv, argc);

  if (commandLine.contains(ihub::plugins::sandbox::cli::pageMapFlag))
    return ihub::plugins::SandboxPageClient::run(commandLine);

  return ihub::plugins::SandboxClient::run(commandLine);
}
