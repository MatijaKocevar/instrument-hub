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

    bool open (int slot, std::string& error);

    bool close (int slot, std::string& error);

    bool isOpen (int slot) const noexcept;
    int openCount() const noexcept;

    void registerInstrument (int slot, plugins::PluginInstrument* instrument);

    void closeBeforeDestroy (AudioEngine::SlotSource& instance) noexcept;

    void closeEditorsExcept (int keepSlot);

    void closeAll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
}
