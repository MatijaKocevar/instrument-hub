#include "Session.h"

#include "session/EditorHost.h"

#include <utility>

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

    // Save the outgoing patch before its instance is retired. This covers the
    // Instrument page's source picker and any API source change.
    (void) capturePluginState (slot);

    const auto& previousSource = config.slots[static_cast<std::size_t> (slot)];
    const bool keepsPluginState =
        previousSource.kind == SessionSlotKind::plugin
        && source.kind == SessionSlotKind::plugin
        && previousSource.path == source.path
        && juce::String (previousSource.pluginId) == juce::String (source.pluginId);

    // A source swap retires the hosted processor: close its editor first,
    // while the instance is still alive, instead of waiting for the unload.
    if (editorHost != nullptr)
    {
        std::string closeError;
        (void) editorHost->close (slot, closeError);
    }

    if (warmPool.getPool() == nullptr || switcher == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    if (! keepsPluginState)
        pluginState.clear (slot);

    // Keep the patch's identity, name, mix and insert chain: only the source
    // fields are taken from the request.
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

    warmPool.replaceRequest (slot, requestId, wasActive, wasRequested);

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

//==============================================================================
// Native plugin editor (T129)

bool Session::openEditor (int slot, std::string& error)
{
    error.clear();

    if (slot < 0 || slot >= static_cast<int> (config.slots.size()))
    {
        error = "patch slot is out of range";
        return false;
    }

    if (editorHost == nullptr)
    {
        error = "no audio session is attached";
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

    if (editorHost == nullptr)
    {
        error = "no audio session is attached";
        return false;
    }

    return editorHost->close (slot, error);
}

bool Session::isEditorOpen (int slot) const noexcept
{
    return editorHost != nullptr && editorHost->isOpen (slot);
}

int Session::getOpenEditorCount() const noexcept
{
    return editorHost != nullptr ? editorHost->openCount() : 0;
}

//==============================================================================
// Control (control thread; never the audio thread)

void Session::armSlot (int slot)
{
    if (switcher == nullptr)
        return;

    if (slot < 0)
    {
        const int previous = warmPool.getActiveSlot();

        if (previous >= 0)
            (void) capturePluginState (previous);

        // Nothing is armed any more: no editor should stay bound to the patch
        // that used to be.
        if (editorHost != nullptr)
            editorHost->closeEditorsExcept (-1);

        warmPool.disarm();
        return;
    }

    if (slot >= getNumSlots())
        return;

    const int previous = warmPool.getActiveSlot();

    if (previous >= 0 && previous != slot)
        (void) capturePluginState (previous);

    // Editor follow: only the armed patch may keep a window. The outgoing
    // editor is closed before the crossfade, so it can never keep showing the
    // previous patch. The new patch's window is opened explicitly later and
    // then shows the state restored by the factory/arm path.
    if (editorHost != nullptr)
        editorHost->closeEditorsExcept (slot);

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

} // namespace ihub
