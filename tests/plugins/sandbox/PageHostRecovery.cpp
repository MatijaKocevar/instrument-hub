#include "PageHostScenario.h"

#include "TestSupport.h"
#include "plugins/PageHost.h"

#include <csignal>
#include <vector>

#if defined(__linux__)
 #include <unistd.h>
#endif

namespace SandboxTest
{
namespace
{
float normaliseFrequency (double hz)
{
    return static_cast<float> ((hz - 20.0) / (20000.0 - 20.0));
}

juce::String hashState (const juce::MemoryBlock& state)
{
    if (state.getSize() == 0)
        return {};

    std::uint64_t hash = 1469598103934665603ULL;
    const auto* bytes = static_cast<const std::uint8_t*> (state.getData());

    for (std::size_t index = 0; index < state.getSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }

    return juce::String::toHexString (static_cast<juce::int64> (hash));
}

bool waitForReady (ihub::plugins::PageHost& host, int previousPid, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes()
                        + static_cast<double> (timeoutMs);

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (host.isReady() && host.getChildProcessId() > 0
            && host.getChildProcessId() != previousPid)
            return true;

        juce::Thread::sleep (10);
    }

    return false;
}
}

void runPageHostRecovery()
{
    ihub::plugins::PageHost::Options options;
    options.pageId = "recovery-test";
    options.helperExecutable = juce::File (IHUB_SANDBOX_HELPER);
    options.startupTimeoutMs = 30000;
    options.hangTimeoutMs = 2000;
    options.controlTimeoutMs = 2000;
    options.recoveryStateTimeoutMs = 5000;
    options.recoveryAttempts = 3;
    options.recoveryBackoffMs = 50;

    const double frequencies[] { 220.0, 330.0, 440.0 };
    const float markers[] { 0.11f, 0.22f, 0.33f };

    for (int slot = 0; slot < 3; ++slot)
    {
        ihub::plugins::PageSlotDescriptor descriptor;
        descriptor.slotIndex = slot;
        descriptor.pluginBundle = juce::File (IHUB_TESTPLUGIN_VST3);
        options.slots.push_back (descriptor);
    }

    auto host = ihub::plugins::PageHost::create (std::move (options));
    juce::String error;

    if (! host->start (error))
    {
        expect (false, "the state page host starts (" + error + ")");
        return;
    }

    std::vector<juce::String> hashes;

    for (int slot = 0; slot < 3; ++slot)
    {
        const std::vector<ihub::plugins::SandboxParameterEdit> edits {
            { "Sine Level", 1.0f },
            { "Gain", 0.0f },
            { "Frequency", normaliseFrequency (frequencies[slot]) },
            { "State Marker", markers[slot] }
        };

        expect (host->setParameters (slot, edits),
                "page slot " + juce::String (slot) + " accepts its preset");
        hashes.push_back (hashState (host->getStateInformation (slot)));
        expect (hashes.back().isNotEmpty(),
                "page slot " + juce::String (slot) + " captures its state");
    }

    const auto oldPid = host->getChildProcessId();

   #if defined(__linux__)
    expect (oldPid > 0 && kill (oldPid, SIGKILL) == 0,
            "the page host child can be killed");
   #endif

    expect (waitForReady (*host, oldPid, 60000),
           "the page host restarts after the child dies");

    for (int slot = 0; slot < 3; ++slot)
        expect (hashState (host->getStateInformation (slot)) == hashes[static_cast<std::size_t> (slot)],
                "page slot " + juce::String (slot) + " restores its committed state");

    const auto restartedPid = host->getChildProcessId();
    juce::String restartError;

    expect (host->restartAndWait (30000, restartError),
           "the manual page restart path succeeds");
    expect (host->getChildProcessId() > 0 && host->getChildProcessId() != restartedPid,
           "the manual page restart replaces the child process");

    for (int slot = 0; slot < 3; ++slot)
        expect (hashState (host->getStateInformation (slot)) == hashes[static_cast<std::size_t> (slot)],
                "page slot " + juce::String (slot) + " keeps its state after a manual restart");

    host->shutdown();
}
}
