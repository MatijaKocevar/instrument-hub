#include "Session.h"

#include "plugins/PageHost.h"
#include "plugins/PageHostSlot.h"

namespace ihub
{

    std::unique_ptr<AudioEngine::SlotSource> Session::createSharedPageSlot (int slotIndex,
                                                                            juce::String& error)
    {
        auto host = ensurePageHost (error);

        if (host == nullptr)
        {
            setSlotError (slotIndex, error);
            return nullptr;
        }

        auto instrument = std::make_unique<plugins::PageHostSlot> (host, slotIndex);

        session::PluginStateIdentity identity;

        if (pluginIdentityForSlot (slotIndex, identity))
        {
            const auto storedState = pluginState.readState (slotIndex, identity);

            if (storedState.getSize () > 0 && !instrument->setStateInformation (storedState))
            {
                juce::Logger::writeToLog ("session: slot " + juce::String (slotIndex) +
                                          " rejected stored plugin state");
            }
        }

        {
            const std::lock_guard<std::mutex> lock (pageSlotHandleMutex);

            if (slotIndex >= static_cast<int> (pageSlotHandles.size ()))
                pageSlotHandles.resize (static_cast<std::size_t> (slotIndex + 1), nullptr);

            pageSlotHandles[static_cast<std::size_t> (slotIndex)] = instrument.get ();
        }

        clearSlotError (slotIndex);
        return instrument;
    }

} // namespace ihub
