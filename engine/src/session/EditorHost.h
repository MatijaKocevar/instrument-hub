#pragma once

#include <functional>
#include <memory>
#include <string>

namespace AudioEngine
{
class SlotSource;
}

namespace ihub
{
struct SessionSlotConfig;

namespace plugins
{
class PluginInstrument;
}

namespace session
{

/**
    Message-thread owner of the native plugin editor windows.

    One window is kept per logical slot. The host is deliberately free of a
    `Session` pointer: it asks the session for a slot's source descriptor and
    live plugin instance through callbacks, and reports a window close so the
    session can capture the patch state. Builds without JUCE's GUI modules
    compile the same interface as a stub that reports a clear error instead of
    dragging a window system into every test binary.
*/
class EditorHost
{
public:
    using SourceLookup = std::function<bool (int slot, SessionSlotConfig& config)>;
    using InstrumentLookup = std::function<plugins::PluginInstrument* (int slot)>;
    using CaptureHook = std::function<void (int slot)>;

    EditorHost (int slotCapacity,
                SourceLookup sourceLookup,
                InstrumentLookup instrumentLookup,
                CaptureHook captureHook);
    ~EditorHost();

    /** Opens, or brings to the front, the editor of `slot`'s live plugin. */
    bool open (int slot, std::string& error);

    /** Closes one slot's window. Idempotent. */
    bool close (int slot, std::string& error);

    bool isOpen (int slot) const noexcept;
    int openCount() const noexcept;

    /** Registers the live plugin instance the warm-pool factory created. */
    void registerInstrument (int slot, plugins::PluginInstrument* instrument);

    /** Pool destroy hook, on the message thread, before the processor dies. */
    void closeBeforeDestroy (AudioEngine::SlotSource& instance) noexcept;

    /** Closes every editor whose slot is not `keepSlot` (pass -1 for all). */
    void closeEditorsExcept (int keepSlot);

    /** Closes every window and forgets every registered instrument. */
    void closeAll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace session
} // namespace ihub
