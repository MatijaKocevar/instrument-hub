#include "controller/Mapping.h"

#include "controller/LaunchpadProtocol.h"

namespace Controller
{

bool Mapping::handleButton (int cc, bool pressed)
{
    if (const auto topRow = Launchpad::topRowIndexForCC (cc))
        return handleTopRow (*topRow, pressed);

    if (const auto rightColumn = Launchpad::rightColumnIndexForCC (cc))
        return handleRightColumn (*rightColumn, pressed);

    return false;
}

bool Mapping::handleTopRow (int index, bool pressed)
{
    if (index < 0 || index >= SetMapping::maximumBanks)
        return false;

    Listeners listenersCopy;
    std::optional<MacroBankSpec> page;
    bool mapped = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
        {
            if (const auto* binding = current->bankBinding (index))
            {
                page = *binding;
                mapped = true;

                if (pressed)
                    activeBank.store (index, std::memory_order_release);
            }
        }
    }

    if (! pressed || ! mapped)
        return false;

    if (listenersCopy.bankEvent)
        listenersCopy.bankEvent (index, &*page);

    return true;
}

bool Mapping::handleRightColumn (int index, bool pressed)
{
    if (index < 0 || index >= SetMapping::maximumScenes)
        return false;

    Listeners listenersCopy;
    std::optional<SceneSpec> selection;
    bool mapped = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
        {
            if (const auto* binding = current->sceneBinding (index))
            {
                selection = *binding;
                mapped = true;

                if (pressed)
                    activeScene.store (index, std::memory_order_release);
            }
        }
    }

    if (! pressed || ! mapped)
        return false;

    if (listenersCopy.sceneEvent)
        listenersCopy.sceneEvent (index, &*selection);

    return true;
}

}
