#include "SharedPageScenario.h"

#include "Session.h"
#include "../concurrent/ConcurrentFixture.h"
#include "plugins/PageHost.h"
#include "plugins/SandboxSlotControl.h"

#include <juce_events/juce_events.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
 #include <unistd.h>
#endif

namespace ConcurrentInstances
{
namespace
{
bool waitForPage (ihub::Session& session, int previousPid, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes()
                        + static_cast<double> (timeoutMs);

    bool sawRecovering = false;

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        const auto statuses = session.getPluginPageStatuses();

        if (! statuses.empty())
        {
            if (statuses.front().status == "recovering")
                sawRecovering = true;

            if (statuses.front().status == "ready"
                && session.getSandboxProcessId (kSlots - 1) > 0
                && session.getSandboxProcessId (kSlots - 1) != previousPid)
            {
                bool allWarm = true;

                for (int slot = 0; slot < kSlots; ++slot)
                    allWarm = allWarm && session.isSlotWarm (slot)
                           && session.getSlotError (slot).isEmpty();

                if (allWarm && sawRecovering)
                    return true;
            }
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (20));
    }

    return false;
}
}

void runSharedPage (const juce::String& bundlePath, const Check& check)
{
    const auto tempRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("ihub-shared-page-"
                                             + juce::String::toHexString (
                                                   juce::Random::getSystemRandom().nextInt64()));

    (void) tempRoot.deleteRecursively();
    tempRoot.createDirectory();

    auto config = makeConfig (bundlePath);
    config.sharedPluginHost = true;

    ihub::Session session;
    session.setPluginStateDirectory (tempRoot.getChildFile ("set.plugin-state"));

    juce::String error;

    if (! session.build (config, error))
    {
        check (false, "the shared plugin page session builds (" + error.toStdString() + ")");
        return;
    }

    check (session.isSharedPluginHostEnabled(),
           "the track page uses the shared plugin host");

    if (! waitForAllWarm (session))
    {
        check (false, "all eight shared page slots warm");
        return;
    }

    const auto statuses = session.getPluginPageStatuses();
    check (statuses.size() == 1,
           "the engine reports exactly one plugin page");
    check (! statuses.empty() && statuses.front().id == "tracks",
           "the plugin page is the track page");
    check (! statuses.empty() && statuses.front().slots.size() == kSlots,
           "the page reports every slot it hosts");
    check (! statuses.empty() && statuses.front().status == "ready",
           "the page starts ready");

    int sharedPid = -1;
    bool sameProcess = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        const auto pid = session.getSandboxProcessId (slot);
        sameProcess = sameProcess && pid > 0;
        sharedPid = sharedPid < 0 ? pid : sharedPid;
        sameProcess = sameProcess && pid == sharedPid;
    }

    check (sameProcess && sharedPid > 0,
           "all eight slots share one plugin host process");

    std::vector<juce::String> hashes;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        applyPreset (session, slot);

        if (auto* control = session.getSandboxSlotControl (slot); control != nullptr)
            hashes.push_back (stateHash (control->getStateInformation()));
        else
            hashes.push_back ({});

        (void) session.capturePluginState (slot);
        check (hashes.back().isNotEmpty(),
               "slot " + std::to_string (slot) + " has a distinct committed state");
    }

    const auto warmCreations = session.getWarmCreationCount();

    for (int slot = 0; slot < kSlots; ++slot)
        (void) measurePitch (session, slot);

    check (session.getWarmCreationCount() == warmCreations,
           "switching every slot performs no plugin load or warming");

    bool everySlotHealthy = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        const auto state = session.getWarmStateName (slot);
        everySlotHealthy = everySlotHealthy && session.isSlotWarm (slot)
                        && std::string (state) != "cold"
                        && std::string (state) != "warming";
    }

    check (everySlotHealthy, "every slot stays warm while the page switches");

    const auto oldPid = session.getSandboxProcessId (kSlots - 1);

   #if defined(__linux__)
    check (oldPid > 0 && kill (oldPid, SIGKILL) == 0,
           "the shared plugin host can be killed for the recovery test");
   #endif

    check (waitForPage (session, oldPid, 90000),
           "killing the shared host recovers the page and every slot");

    const auto restartedPid = session.getSandboxProcessId (kSlots - 1);
    check (restartedPid > 0 && restartedPid != oldPid,
           "recovery starts a new shared plugin host process");

    bool statesRestored = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (auto* control = session.getSandboxSlotControl (slot); control != nullptr)
            statesRestored = statesRestored
                          && stateHash (control->getStateInformation()) == hashes[static_cast<std::size_t> (slot)];
        else
            statesRestored = false;
    }

    check (statesRestored, "recovery restores every slot's committed state");

    const auto recoveredPitch = measurePitch (session, kSlots - 1);
    check (std::abs (recoveredPitch - kFrequencies[kSlots - 1]) <= 20.0,
           "recovery re-arms the track that was active before the failure");

    std::string restartError;
    check (session.restartPluginPage ("tracks", restartError),
           "a manual page restart uses the recovery path"
               + (restartError.empty() ? std::string() : " (" + restartError + ")"));

    const auto manualPid = session.getSandboxProcessId (kSlots - 1);
    check (manualPid > 0 && manualPid != restartedPid,
           "the manual restart replaces the shared host process");

    bool stillRestored = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (auto* control = session.getSandboxSlotControl (slot); control != nullptr)
            stillRestored = stillRestored
                         && stateHash (control->getStateInformation()) == hashes[static_cast<std::size_t> (slot)];
        else
            stillRestored = false;
    }

    check (stillRestored, "the manual restart keeps every slot's state");

    const auto finalStatuses = session.getPluginPageStatuses();
    check (! finalStatuses.empty() && finalStatuses.front().status == "ready",
           "the page returns to ready after the manual restart");

    const auto beforeReloadPid = session.getSandboxProcessId (kSlots - 1);
    std::string reloadError;
    check (session.reloadPlugin (kSlots - 1, reloadError),
           "the patch reload route restarts the shared page"
               + (reloadError.empty() ? std::string() : " (" + reloadError + ")"));

    bool reloadRestored = true;

    for (int slot = 0; slot < kSlots; ++slot)
    {
        if (auto* control = session.getSandboxSlotControl (slot); control != nullptr)
            reloadRestored = reloadRestored
                          && stateHash (control->getStateInformation()) == hashes[static_cast<std::size_t> (slot)];
        else
            reloadRestored = false;
    }

    check (session.getSandboxProcessId (kSlots - 1) > 0
               && session.getSandboxProcessId (kSlots - 1) != beforeReloadPid,
           "the patch reload route replaces the shared host process");
    check (reloadRestored, "the patch reload route keeps every slot's state");

    (void) tempRoot.deleteRecursively();
}

} // namespace ConcurrentInstances
