#include "Session.h"

#include "plugins/PageHost.h"
#include "plugins/SandboxHost.h"

namespace ihub
{
namespace
{
constexpr int reloadTimeoutMs = 30000;
constexpr int pageReloadTimeoutMs = 60000;
}

bool Session::reloadPlugin (int slot, std::string& error)
{
    error.clear();

    if (slot < 0 || slot >= getLogicalSlotCount())
    {
        error = "patch slot is out of range";
        return false;
    }

    session::PluginStateIdentity identity;

    if (! pluginIdentityForSlot (slot, identity))
    {
        error = "patch slot does not host a plugin";
        return false;
    }

    if (sharedPluginHostEnabled)
    {
        auto page = getPageHost();

        if (page == nullptr || ! page->hasSlot (slot))
        {
            error = "the patch is not hosted by the shared plugin host";
            return false;
        }

        juce::String pageError;

        if (! page->restartAndWait (pageReloadTimeoutMs, pageError))
        {
            error = pageError.isNotEmpty()
                        ? pageError.toStdString()
                        : std::string ("the shared plugin host did not restart");
            return false;
        }

        clearSlotError (slot);
        retryPageWarmSlots();
        notifySlotStateChanged (slot);
        return true;
    }

    auto* host = sandboxForSlot (slot);
    const bool wasHealthy = host != nullptr
                         && host->isRunning()
                         && ! host->isFailed();

    if (host != nullptr && host->isEditorOpen())
    {
        juce::String editorError;

        if (! host->closeEditor (editorError))
            juce::Logger::writeToLog ("session: slot " + juce::String (slot)
                                      + " could not close its editor before reload: "
                                      + editorError);
    }

    if (wasHealthy)
        (void) capturePluginState (slot);

    (void) bumpSandboxGeneration (slot);

    if (host != nullptr)
        host->stop();

    juce::String requestId;

    {
        const std::lock_guard<std::mutex> lock (sourceMutex);

        const auto& source = sourceConfigs[static_cast<std::size_t> (slot)];
        requestId = juce::String (slot) + ":" + toString (source.kind) + ":"
                    + source.path + ":" + juce::String (source.frequency);
    }

    std::uint64_t reloadRevision = 0;

    {
        const std::lock_guard<std::mutex> lock (sandboxMutex);

        if (slot >= static_cast<int> (pluginReloadRevisions.size()))
            pluginReloadRevisions.resize (static_cast<std::size_t> (slot + 1), 0);

        reloadRevision = ++pluginReloadRevisions[static_cast<std::size_t> (slot)];
    }

    requestId += ":reload:" + juce::String (reloadRevision);

    const bool wasActive = warmPool.getActiveSlot() == slot;
    const bool wasRequested = warmPool.getRequestedSlot() == slot;

    warmPool.replaceRequest (slot, requestId, wasActive, wasRequested);

    if (! warmPool.waitForSlotWarm (slot, reloadTimeoutMs))
    {
        error = "the plugin did not restart";

        if (auto* failedHost = sandboxForSlot (slot); failedHost != nullptr
                                                       && failedHost->isFailed())
        {
            const auto reason = failedHost->getFailureReason();

            if (reason.isNotEmpty())
                error = reason.toStdString();
        }

        setSlotError (slot, juce::String (error));
        return false;
    }

    if (auto* restarted = sandboxForSlot (slot); restarted != nullptr
                                                   && restarted->isFailed())
    {
        const auto reason = restarted->getFailureReason();
        error = reason.isNotEmpty() ? reason.toStdString()
                                    : "the plugin reported a failure after restart";
        setSlotError (slot, juce::String (error));
        return false;
    }

    clearSlotError (slot);
    notifySlotStateChanged (slot);
    return true;
}

} // namespace ihub
