#include "controller/Mapping.h"

#include <utility>

namespace Controller
{

namespace
{
bool bindingsEqual (const PadBinding* left, const PadBinding* right) noexcept
{
    if (left == nullptr || right == nullptr)
        return left == right;

    return *left == *right;
}

} // namespace

bool Mapping::applyText (const std::string& text,
                         bool hotReload,
                         std::uint64_t contentHash,
                         bool force,
                         MappingError* error)
{
    SetMapping parsed;
    MappingError parseError;

    if (! parseSetMappingYaml (text, parsed, parseError))
    {
        reportError (parseError, contentHash, true);

        if (error != nullptr)
            *error = parseError;

        return false;
    }

    auto newMapping = std::make_shared<SetMapping> (std::move (parsed));

    Listeners listenersCopy;
    std::shared_ptr<const SetMapping> oldMapping;
    bool routeChanged = false;
    int armTarget = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (! force && hasAttempted && contentHash == lastHash)
        {
            if (error != nullptr)
                error->message.clear();

            return false;
        }

        const int active = activePad.load (std::memory_order_acquire);
        oldMapping = current;

        const PadBinding* oldBinding = (oldMapping != nullptr && active >= 0)
                                           ? oldMapping->padBinding (active)
                                           : nullptr;
        const PadBinding* newBinding = active >= 0 ? newMapping->padBinding (active) : nullptr;

        routeChanged = ! bindingsEqual (oldBinding, newBinding);

        if (newBinding != nullptr)
            armTarget = newBinding->slot;

        if (newMapping->bankBinding (activeBank.load (std::memory_order_relaxed)) == nullptr)
            activeBank.store (-1, std::memory_order_release);

        if (newMapping->sceneBinding (activeScene.load (std::memory_order_relaxed)) == nullptr)
            activeScene.store (-1, std::memory_order_release);

        current = newMapping;
        lastError = {};
        lastHash = contentHash;
        hasAttempted = true;
        hasLoaded = true;
        revision.fetch_add (1, std::memory_order_release);
        listenersCopy = listeners;
    }

    if (error != nullptr)
        error->message.clear();

    if (listenersCopy.changed)
        listenersCopy.changed (newMapping, hotReload);

    // Re-arming happens after the snapshot is published, so a listener that
    // reads getMapping() sees the document it is being notified about.
    if (routeChanged && listenersCopy.armSlot)
        listenersCopy.armSlot (armTarget);

    return true;
}

void Mapping::reportError (const MappingError& error, std::uint64_t contentHash, bool rememberHash)
{
    Listeners listenersCopy;
    bool shouldNotify = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        shouldNotify = lastError.message != error.message || lastError.line != error.line;
        lastError = error;

        if (rememberHash)
        {
            lastHash = contentHash;
            hasAttempted = true;
        }

        listenersCopy = listeners;
    }

    if (shouldNotify && listenersCopy.error)
        listenersCopy.error (error);
}

} // namespace Controller
