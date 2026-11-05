#include "PageHostScenario.h"

#include "TestSupport.h"
#include "plugins/PageHost.h"

namespace SandboxTest
{
void runPageHostFailedRestart()
{
    ihub::plugins::PageHost::Options options;
    options.pageId = "failed-restart-test";
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.startupTimeoutMs = 5000;
    options.recoveryAttempts = 1;
    options.recoveryBackoffMs = 10;

    ihub::plugins::PageSlotDescriptor descriptor;
    descriptor.slotIndex = 0;
    descriptor.pluginBundle = juce::File ("/nonexistent/instrument-hub-test-plugin.vst3");
    options.slots.push_back (descriptor);

    auto host = ihub::plugins::PageHost::create (std::move (options));
    juce::String error;
    const bool started = host->start (error);

    expect (! started, "a page with a missing plugin fails to start");
    expect (host->getStatus() == ihub::plugins::PageHost::Status::failed,
            "the failed page reports failed");

    juce::String restartError;
    expect (! host->restartAndWait (5000, restartError),
            "a manual restart of a failed page fails cleanly");
    expect (host->getStatus() == ihub::plugins::PageHost::Status::failed,
            "the failed page stays failed after the retry budget");
    expect (host->getFailureReason().isNotEmpty(),
            "the failed page keeps a readable reason");

    host->shutdown();
    expect (host->getStatus() == ihub::plugins::PageHost::Status::stopped,
            "the failed page shuts down cleanly");
}
}
