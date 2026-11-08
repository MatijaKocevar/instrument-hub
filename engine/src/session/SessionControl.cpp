#include "Session.h"

#include "plugins/PageHost.h"
#include "plugins/SandboxHost.h"
#include "session/EditorHost.h"

#include <utility>
#include <vector>

namespace ihub
{

bool Session::setSlotSource (int slot,
                             const SessionSlotConfig& source,
                             std::string& error)
{
    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "no audio session is attached";
        return false;
    }

    (void) capturePluginState (slot);

    const auto previousSource = config.slots[static_cast<std::size_t> (slot)];
    const bool keepsPluginState =
        previousSource.kind == SessionSlotKind::plugin
        && source.kind == SessionSlotKind::plugin
        && previousSource.path == source.path
        && juce::String (previousSource.pluginId) == juce::String (source.pluginId);

    if (editorHost != nullptr)
    {
        std::string closeError;
        (void) editorHost->close (slot, closeError);
    }

    closeSandboxEditors (-1);

    releasePluginMirror (slot);

    if (warmPool.getPool() == nullptr || switcher == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    if (! keepsPluginState)
        pluginState.clear (slot);

    auto& current = config.slots[static_cast<std::size_t> (slot)];
    current.kind = source.kind;
    current.path = source.path;
    current.frequency = source.frequency >= 20.0 ? source.frequency
                                                 : current.frequency;
    current.inputChannel = source.inputChannel;
    current.instrumentKind = source.instrumentKind.empty()
                                 ? toString (source.kind)
                                 : source.instrumentKind;
    current.pluginFormat = source.pluginFormat;
    current.pluginId = source.pluginId;
    current.pluginName = source.pluginName;
    current.pluginManufacturer = source.pluginManufacturer;
    current.pluginVersion = source.pluginVersion;
    current.samplePath = source.samplePath;

    {
        const std::lock_guard<std::mutex> sourceLock (sourceMutex);

        if (slot < static_cast<int> (sourceConfigs.size()))
            sourceConfigs[static_cast<std::size_t> (slot)] = current;
    }

    const bool wasActive = warmPool.getActiveSlot() == slot;
    const bool wasRequested = warmPool.getRequestedSlot() == slot;

    const auto requestId = juce::String (slot) + ":" + toString (current.kind) + ":"
                         + current.path + ":" + juce::String (current.frequency) + ":"
                         + juce::String (++sourceRevision);

    const bool wasPlugin = previousSource.kind == SessionSlotKind::plugin;
    const bool isPlugin = current.kind == SessionSlotKind::plugin;
    const bool pluginSetChanged = wasPlugin != isPlugin
        || (wasPlugin && isPlugin
            && (previousSource.path != current.path
                || juce::String (previousSource.pluginId) != juce::String (current.pluginId)));

    std::vector<int> previousPageSlots;

    if (sharedPluginHostEnabled && pluginSetChanged)
    {
        if (auto page = getPageHost(); page != nullptr)
            previousPageSlots = page->getSlotIndices();

        {
            const std::lock_guard<std::mutex> lock (pageSlotHandleMutex);

            for (const int pageSlot : previousPageSlots)
                if (pageSlot >= 0 && pageSlot < static_cast<int> (pageSlotHandles.size()))
                    pageSlotHandles[static_cast<std::size_t> (pageSlot)] = nullptr;

            if (slot >= 0 && slot < static_cast<int> (pageSlotHandles.size()))
                pageSlotHandles[static_cast<std::size_t> (slot)] = nullptr;
        }

        resetPageHost();
    }

    if (current.kind == SessionSlotKind::empty)
        warmPool.setRequest (slot, {});
    else
        warmPool.replaceRequest (slot, requestId, wasActive, wasRequested);

    if (sharedPluginHostEnabled && pluginSetChanged)
    {
        for (const int other : previousPageSlots)
        {
            if (other == slot)
                continue;

            const auto& otherSource = config.slots[static_cast<std::size_t> (other)];

            if (otherSource.kind != SessionSlotKind::plugin)
                continue;

            const auto otherRequestId = juce::String (other) + ":"
                                      + toString (otherSource.kind) + ":"
                                      + otherSource.path + ":"
                                      + juce::String (otherSource.frequency) + ":"
                                      + juce::String (++sourceRevision);

            warmPool.replaceRequest (other,
                                     otherRequestId,
                                     warmPool.getActiveSlot() == other,
                                     warmPool.getRequestedSlot() == other);
        }
    }

    SourceAssignedHook hook;

    {
        const std::lock_guard<std::mutex> hookLock (sourceAssignedMutex);
        hook = sourceAssignedHook;
    }

    if (hook)
        hook (slot, config.slots[static_cast<std::size_t> (slot)]);

    return true;
}

void Session::setSourceAssignedHook (SourceAssignedHook hook)
{
    const std::lock_guard<std::mutex> lock (sourceAssignedMutex);
    sourceAssignedHook = std::move (hook);
}

bool Session::openEditor (int slot, std::string& error)
{
    error.clear();

    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "patch slot is out of range";
        return false;
    }

    if (isWineHostedSlot (slot))
    {
        if (auto* sandbox = getSandboxSlotControl (slot))
        {
            juce::String editorError;

            if (! sandbox->openEditor (editorError))
            {
                error = editorError.isNotEmpty()
                            ? editorError.toStdString()
                            : "the plugin host could not open the editor";
                setSlotError (slot, juce::String (error));
                return false;
            }

            clearSlotError (slot);
            return true;
        }
    }

    if (editorHost == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    if (pluginState.getInstrument (slot) == nullptr)
    {
        session::PluginStateIdentity identity;

        if (pluginIdentityForSlot (slot, identity)
            && ! ensurePluginMirror (slot, error))
            return false;
    }

    return editorHost->open (slot, error);
}

bool Session::closeEditor (int slot, std::string& error)
{
    error.clear();

    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "patch slot is out of range";
        return false;
    }

    if (isWineHostedSlot (slot))
    {
        if (auto* sandbox = getSandboxSlotControl (slot))
        {
            juce::String editorError;

            const auto revisionBefore = sandbox->getLastCommittedStateRevision();

            if (! sandbox->closeEditor (editorError))
            {
                error = editorError.isNotEmpty()
                            ? editorError.toStdString()
                            : "the plugin host could not close the editor";
                setSlotError (slot, juce::String (error));
                return false;
            }

            (void) sandbox->flushPublishedState();

            if (sandbox->getLastCommittedStateRevision() == revisionBefore)
                (void) capturePluginState (slot);

            clearSlotError (slot);
            return true;
        }
    }

    if (editorHost == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    return editorHost->close (slot, error);
}

bool Session::isEditorOpen (int slot) const noexcept
{
    if (isWineHostedSlot (slot))
        if (auto* sandbox = getSandboxSlotControl (slot); sandbox != nullptr)
            return sandbox->isEditorOpen();

    return editorHost != nullptr && editorHost->isOpen (slot);
}

int Session::getOpenEditorCount() const noexcept
{
    int count = editorHost != nullptr ? editorHost->openCount() : 0;

    {
        const std::lock_guard<std::mutex> lock (sandboxMutex);

        for (const auto& entry : sandboxSlots)
            if (entry.host != nullptr && entry.host->isEditorOpen())
                ++count;
    }

    if (auto host = getPageHost(); host != nullptr)
        count += host->getOpenEditorCount();

    return count;
}

bool Session::isWineHostedSlot (int slot) const
{
    session::PluginStateIdentity identity;

    return pluginIdentityForSlot (slot, identity)
        && identity.pluginPath.containsIgnoreCase ("vstbridge");
}

void Session::closeSandboxEditors (int keepSlot)
{
    std::vector<plugins::SandboxSlotControl*> hosts;

    {
        const std::lock_guard<std::mutex> lock (sandboxMutex);

        for (const auto& entry : sandboxSlots)
            if (entry.slot != keepSlot && entry.host != nullptr
                && entry.host->isEditorOpen())
                hosts.push_back (entry.host);
    }

    if (auto page = getPageHost(); page != nullptr)
    {
        for (const int slot : page->getSlotIndices())
        {
            if (slot == keepSlot)
                continue;

            if (auto* control = getSandboxSlotControl (slot);
                control != nullptr && control->isEditorOpen())
                hosts.push_back (control);
        }
    }

    for (auto* host : hosts)
    {
        juce::String error;
        (void) host->closeEditor (error);
    }
}

void Session::armSlot (int slot)
{
    if (switcher == nullptr)
        return;

    if (slot < 0)
    {
        if (editorHost != nullptr)
            editorHost->closeEditorsExcept (-1);

        closeSandboxEditors (-1);

        if (auto page = getPageHost(); page != nullptr)
        {
            for (const int pageSlot : page->getSlotIndices())
                page->allNotesOff (pageSlot);

            page->setActiveSlot (-1);
        }

        warmPool.disarm();
        return;
    }

    if (slot >= getNumSlots())
        return;

    if (auto page = getPageHost(); page != nullptr)
    {
        for (const int pageSlot : page->getSlotIndices())
            page->allNotesOff (pageSlot);

        page->setActiveSlot (slot);
    }

    if (editorHost != nullptr)
        editorHost->closeEditorsExcept (slot);

    closeSandboxEditors (slot);

    warmPool.arm (slot);
}

int Session::getArmedSlot() const noexcept
{
    return warmPool.getRequestedSlot();
}

void Session::setSlotGain (int slot, float linearGain) noexcept
{
    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
        return;

    const float clamped = juce::jlimit (0.0f, 4.0f, linearGain);
    config.slots[static_cast<std::size_t> (slot)].gain = clamped;

    if (engine != nullptr && slot < engine->getNumSlots())
        engine->getSlot (slot).setGain (clamped);
}

}
