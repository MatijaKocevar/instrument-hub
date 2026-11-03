#include "EditorHost.h"

#include "Session.h"
#include "plugins/PluginInstrument.h"

#if defined (JUCE_MODULE_AVAILABLE_juce_gui_basics) && JUCE_MODULE_AVAILABLE_juce_gui_basics
 #include "plugins/PluginWindow.h"
 #define IHUB_SESSION_HAS_EDITORS 1
#else
 #define IHUB_SESSION_HAS_EDITORS 0
#endif

#include <array>
#include <atomic>
#include <mutex>
#include <utility>
#include <vector>

namespace ihub::session
{

#if IHUB_SESSION_HAS_EDITORS
struct EditorHost::Impl final
{
    Impl (int capacity,
          SourceLookup source,
          InstrumentLookup instrument,
          CaptureHook capture)
        : slotCapacity (capacity),
          sourceForSlot (std::move (source)),
          instrumentForSlot (std::move (instrument)),
          captureState (std::move (capture)),
          instruments (static_cast<std::size_t> (capacity), nullptr),
          windows (static_cast<std::size_t> (capacity)),
          openFlags (static_cast<std::size_t> (capacity))
    {
    }

    bool open (int slot, std::string& error)
    {
        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager == nullptr)
        {
            error = "the message loop is not running; cannot create a native editor";
            return false;
        }

        if (! manager->isThisTheMessageThread() && isPluginSlot (slot))
        {
            const double deadline = juce::Time::getMillisecondCounterHiRes() + 20000.0;

            while (findInstrument (slot) == nullptr
                   && juce::Time::getMillisecondCounterHiRes() < deadline)
                juce::Thread::sleep (5);
        }

        auto result = juce::MessageManager::callSync ([this, slot]
        {
            return openOnMessageThread (slot);
        });

        if (! result.has_value())
        {
            error = "could not reach the message thread";
            return false;
        }

        error = std::move (result->second);
        return result->first;
    }

    bool close (int slot, std::string& error)
    {
        if (slot < 0 || slot >= slotCapacity)
        {
            error = "patch slot is out of range";
            return false;
        }

        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager == nullptr)
        {
            error = "the message loop is not running; cannot close a native editor";
            return false;
        }

        auto result = juce::MessageManager::callSync ([this, slot]
        {
            return closeOnMessageThread (slot);
        });

        if (! result.has_value())
        {
            error = "could not reach the message thread";
            return false;
        }

        error = std::move (result->second);
        return result->first;
    }

    bool isOpen (int slot) const noexcept
    {
        return slot >= 0 && slot < slotCapacity
            && openFlags[static_cast<std::size_t> (slot)].load (std::memory_order_acquire);
    }

    int openCount() const noexcept
    {
        return openWindowCount.load (std::memory_order_acquire);
    }

    void registerInstrument (int slot, plugins::PluginInstrument* instrument)
    {
        if (instrument == nullptr || slot < 0 || slot >= slotCapacity)
            return;

        const std::lock_guard<std::mutex> lock (instrumentMutex);
        instruments[static_cast<std::size_t> (slot)] = instrument;
    }

    void closeBeforeDestroy (AudioEngine::SlotSource& instance) noexcept
    {
        auto* instrument = dynamic_cast<plugins::PluginInstrument*> (&instance);

        if (instrument == nullptr)
            return;

        {
            const std::lock_guard<std::mutex> lock (instrumentMutex);

            for (auto& entry : instruments)
                if (entry == instrument)
                    entry = nullptr;
        }

        auto slot = findSlotForProcessor (instrument->getPlugin());

        if (slot >= 0)
            (void) closeOnMessageThread (slot);
    }

    void closeAll()
    {
        bool anyOpen = openWindowCount.load (std::memory_order_acquire) > 0;

        for (const auto& window : windows)
            anyOpen = anyOpen || (window != nullptr && window->isOpen());

        if (! anyOpen)
        {
            for (auto& flag : openFlags)
                flag.store (false, std::memory_order_release);

            openWindowCount.store (0, std::memory_order_release);

            const std::lock_guard<std::mutex> lock (instrumentMutex);

            for (auto& entry : instruments)
                entry = nullptr;

            return;
        }

        auto closeHere = [this]
        {
            for (auto& window : windows)
            {
                if (window == nullptr)
                    continue;

                window->onClose = nullptr;
                window->onBeforeClose = nullptr;
                window->close();
                window.reset();
            }

            for (auto& flag : openFlags)
                flag.store (false, std::memory_order_release);

            openWindowCount.store (0, std::memory_order_release);

            const std::lock_guard<std::mutex> lock (instrumentMutex);

            for (auto& entry : instruments)
                entry = nullptr;
        };

        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager != nullptr && ! manager->isThisTheMessageThread())
            juce::MessageManager::callSync (closeHere);
        else
            closeHere();
    }

    void closeEditorsExcept (int keepSlot)
    {
        auto* manager = juce::MessageManager::getInstanceWithoutCreating();

        if (manager == nullptr)
            return;

        auto closeHere = [this, keepSlot]
        {
            for (int slot = 0; slot < static_cast<int> (windows.size()); ++slot)
            {
                if (slot == keepSlot)
                    continue;

                auto& window = windows[static_cast<std::size_t> (slot)];

                if (window != nullptr && window->isOpen())
                    window->close();
            }
        };

        if (! manager->isThisTheMessageThread())
            juce::MessageManager::callSync (closeHere);
        else
            closeHere();
    }

private:
    bool isPluginSlot (int slot) const
    {
        SessionSlotConfig config;
        return sourceForSlot(slot, config) && config.kind == SessionSlotKind::plugin;
    }

    bool readSlotConfig (int slot, SessionSlotConfig& config) const
    {
        return sourceForSlot(slot, config);
    }

    std::pair<bool, std::string> openOnMessageThread (int slot)
    {
        SessionSlotConfig slotConfig;

        if (! readSlotConfig (slot, slotConfig))
            return { false, "patch slot is out of range" };

        const auto label = "slot " + std::to_string (slot);

        if (slotConfig.kind != SessionSlotKind::plugin)
            return { false, label + " is a " + toString (slotConfig.kind)
                                + " source; only plugin patches have a native editor" };

        auto* instrument = findInstrument (slot);

        if (instrument == nullptr)
            return { false, "the plugin for " + label
                                + " is not loaded; arm the patch and wait for it to warm" };

        auto* processor = instrument->getPlugin();

        if (processor == nullptr)
            return { false, "the plugin for " + label + " has no live instance" };

        auto& window = windows[static_cast<std::size_t> (slot)];

        if (window == nullptr)
        {
            window = std::make_unique<plugins::PluginWindow>();

            window->onBeforeClose = [this, slot]
            {
                if (captureState)
                    captureState (slot);
            };

            window->onClose = [this, slot]
            {
                const auto index = static_cast<std::size_t> (slot);
                openFlags[index].store (false, std::memory_order_release);
                openWindowCount.fetch_sub (1, std::memory_order_acq_rel);
            };
        }
        else if (window->isOpen())
        {
            if (window->getProcessor() == processor)
            {
                if (auto* component = window->getWindowComponent())
                    component->toFront (true);

                return { true, {} };
            }

            (void) closeOnMessageThread (slot);
        }

        juce::String windowError;
        const juce::String title = slotConfig.pluginName.empty()
                                       ? juce::String (slotConfig.name)
                                       : juce::String (slotConfig.pluginName);

        if (! window->open (*instrument, title, nullptr, &windowError))
        {
            auto message = windowError.isNotEmpty()
                               ? windowError.toStdString()
                               : std::string ("the plugin did not create an editor");

            return { false, label + ": " + message };
        }

        openFlags[static_cast<std::size_t> (slot)].store (true, std::memory_order_release);
        openWindowCount.fetch_add (1, std::memory_order_acq_rel);

        return { true, {} };
    }

    std::pair<bool, std::string> closeOnMessageThread (int slot)
    {
        if (slot < 0 || slot >= static_cast<int> (windows.size()))
            return { false, "patch slot is out of range" };

        auto& window = windows[static_cast<std::size_t> (slot)];

        if (window != nullptr && window->isOpen())
            window->close();

        return { true, {} };
    }

    plugins::PluginInstrument* findInstrument (int slot) const
    {
        if (slot < 0 || slot >= slotCapacity)
            return nullptr;

        const std::lock_guard<std::mutex> lock (instrumentMutex);
        return instruments[static_cast<std::size_t> (slot)];
    }

    int findSlotForProcessor (juce::AudioProcessor* processor) const
    {
        if (processor == nullptr)
            return -1;

        for (int slot = 0; slot < static_cast<int> (windows.size()); ++slot)
            if (windows[static_cast<std::size_t> (slot)] != nullptr
                && windows[static_cast<std::size_t> (slot)]->isOpen()
                && windows[static_cast<std::size_t> (slot)]->getProcessor() == processor)
                return slot;

        return -1;
    }

    const int slotCapacity;
    SourceLookup sourceForSlot;
    InstrumentLookup instrumentForSlot;
    CaptureHook captureState;

    mutable std::mutex instrumentMutex;
    std::vector<plugins::PluginInstrument*> instruments;

    std::vector<std::unique_ptr<plugins::PluginWindow>> windows;

    std::vector<std::atomic<bool>> openFlags;
    std::atomic<int> openWindowCount { 0 };
};
#else
struct EditorHost::Impl final
{
    Impl (int capacity,
          SourceLookup,
          InstrumentLookup,
          CaptureHook)
        : slotCapacity (capacity)
    {
    }

    bool open (int slot, std::string& error)
    {
        if (slot < 0 || slot >= slotCapacity)
        {
            error = "patch slot is out of range";
            return false;
        }

        error = "native plugin editors are not available in this build";
        return false;
    }

    bool close (int slot, std::string& error)
    {
        if (slot < 0 || slot >= slotCapacity)
        {
            error = "patch slot is out of range";
            return false;
        }

        error.clear();
        return true;
    }

    bool isOpen (int) const noexcept { return false; }
    int openCount() const noexcept { return 0; }
    void registerInstrument (int, plugins::PluginInstrument*) {}
    void closeBeforeDestroy (AudioEngine::SlotSource&) noexcept {}
    void closeEditorsExcept (int) {}
    void closeAll() {}

    const int slotCapacity;
};
#endif

EditorHost::EditorHost (int slotCapacity,
                        SourceLookup sourceLookup,
                        InstrumentLookup instrumentLookup,
                        CaptureHook captureHook)
    : impl (std::make_unique<Impl> (slotCapacity,
                                    std::move (sourceLookup),
                                    std::move (instrumentLookup),
                                    std::move (captureHook)))
{
}

EditorHost::~EditorHost() = default;

bool EditorHost::open (int slot, std::string& error)
{
    return impl->open (slot, error);
}

bool EditorHost::close (int slot, std::string& error)
{
    return impl->close (slot, error);
}

bool EditorHost::isOpen (int slot) const noexcept
{
    return impl->isOpen (slot);
}

int EditorHost::openCount() const noexcept
{
    return impl->openCount();
}

void EditorHost::closeBeforeDestroy (AudioEngine::SlotSource& instance) noexcept
{
    impl->closeBeforeDestroy (instance);
}

void EditorHost::closeEditorsExcept (int keepSlot)
{
    impl->closeEditorsExcept (keepSlot);
}

void EditorHost::closeAll()
{
    impl->closeAll();
}

void EditorHost::registerInstrument (int slot, plugins::PluginInstrument* instrument)
{
    impl->registerInstrument (slot, instrument);
}

}
