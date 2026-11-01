#include "Mapping.h"

#include "MappingYamlInternal.h"
#include "LaunchpadProtocol.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <system_error>
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

//==============================================================================
// Mapping
//==============================================================================

Mapping::Mapping() = default;

Mapping::~Mapping()
{
    stopWatching();
}

void Mapping::setListeners (Listeners newListeners)
{
    const std::lock_guard<std::mutex> lock (mutex);
    listeners = std::move (newListeners);
}

bool Mapping::load (const std::filesystem::path& file, MappingError* error)
{
    std::error_code ec;
    auto absolute = std::filesystem::absolute (file, ec);

    if (ec)
        absolute = file;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        path = std::move (absolute);
        lastHash = 0;
        hasAttempted = false;
        hasLoaded = false;
        activePad.store (-1, std::memory_order_release);
        activeBank.store (-1, std::memory_order_release);
        activeScene.store (-1, std::memory_order_release);
    }

    return reloadInternal (false, error);
}

bool Mapping::reload (MappingError* error)
{
    const bool hot = [this]
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return hasLoaded;
    }();

    return reloadInternal (hot, error);
}

bool Mapping::reloadInternal (bool hotReload, MappingError* error)
{
    std::filesystem::path currentPath;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentPath = path;
    }

    if (currentPath.empty())
    {
        const MappingError failure { 0, "no set file has been loaded" };
        reportError (failure, 0, false);

        if (error != nullptr)
            *error = failure;

        return false;
    }

    std::string contents;
    std::string readError;

    if (! MappingDetail::readFileContents (currentPath, contents, readError))
    {
        const MappingError failure { 0, readError };
        reportError (failure, 0, false);

        if (error != nullptr)
            *error = failure;

        return false;
    }

    return applyText (contents, hotReload, MappingDetail::hashContents (contents), true, error);
}

bool Mapping::poll()
{
    std::filesystem::path currentPath;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        currentPath = path;
    }

    if (currentPath.empty())
        return false;

    std::string contents;
    std::string readError;

    if (! MappingDetail::readFileContents (currentPath, contents, readError))
    {
        const MappingError failure { 0, readError };
        reportError (failure, 0, false);
        return false;
    }

    const auto hash = MappingDetail::hashContents (contents);
    bool hot = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);

        if (hasAttempted && hash == lastHash)
            return false;

        hot = hasLoaded;
    }

    return applyText (contents, hot, hash, false, nullptr);
}

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

void Mapping::watchLoop (int intervalMs)
{
    while (! quitWatcher.load (std::memory_order_acquire))
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (intervalMs));

        if (quitWatcher.load (std::memory_order_acquire))
            break;

        poll();
    }

    watching.store (false, std::memory_order_release);
}

bool Mapping::startWatching (int intervalMs)
{
    if (intervalMs < 5 || intervalMs > 5000)
        return false;

    const std::lock_guard<std::mutex> lock (watchMutex);

    if (watcher.joinable())
        return true;

    quitWatcher.store (false, std::memory_order_release);
    watching.store (true, std::memory_order_release);

    try
    {
        watcher = std::thread ([this, intervalMs] { watchLoop (intervalMs); });
    }
    catch (const std::system_error&)
    {
        watching.store (false, std::memory_order_release);
        return false;
    }

    return true;
}

void Mapping::stopWatching()
{
    quitWatcher.store (true, std::memory_order_release);

    const std::lock_guard<std::mutex> lock (watchMutex);

    if (! watcher.joinable())
    {
        watching.store (false, std::memory_order_release);
        return;
    }

    // A listener invoked by the watcher thread must not join itself; the
    // watcher exits its loop after this callback and a later stop/join cleans
    // the thread up.
    if (watcher.get_id() == std::this_thread::get_id())
        return;

    watcher.join();
    watching.store (false, std::memory_order_release);
}

bool Mapping::isWatching() const noexcept
{
    return watching.load (std::memory_order_acquire);
}

std::filesystem::path Mapping::getPath() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return path;
}

std::shared_ptr<const SetMapping> Mapping::getMapping() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return current;
}

std::uint64_t Mapping::getRevision() const noexcept
{
    return revision.load (std::memory_order_acquire);
}

MappingError Mapping::getLastError() const
{
    const std::lock_guard<std::mutex> lock (mutex);
    return lastError;
}

std::optional<PadBinding> Mapping::getPadBinding (int pad) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* binding = current->padBinding (pad))
        return *binding;

    return std::nullopt;
}

std::optional<EncoderBinding> Mapping::getEncoderBinding (int encoder) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* binding = current->encoderBinding (encoder))
        return *binding;

    return std::nullopt;
}

std::optional<MacroBankSpec> Mapping::getBank (int bank) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* page = current->bankBinding (bank))
        return *page;

    return std::nullopt;
}

std::optional<SceneSpec> Mapping::getScene (int scene) const
{
    const std::lock_guard<std::mutex> lock (mutex);

    if (current == nullptr)
        return std::nullopt;

    if (const auto* selection = current->sceneBinding (scene))
        return *selection;

    return std::nullopt;
}

int Mapping::getActivePad() const noexcept
{
    return activePad.load (std::memory_order_acquire);
}

int Mapping::getActiveBank() const noexcept
{
    return activeBank.load (std::memory_order_acquire);
}

int Mapping::getActiveScene() const noexcept
{
    return activeScene.load (std::memory_order_acquire);
}

int Mapping::getArmedSlot() const noexcept
{
    const std::lock_guard<std::mutex> lock (mutex);
    const int pad = activePad.load (std::memory_order_relaxed);

    if (current != nullptr && pad >= 0)
        if (const auto* binding = current->padBinding (pad))
            return binding->slot;

    return -1;
}

bool Mapping::handlePad (int pad, bool pressed)
{
    if (pad < 0 || pad >= SetMapping::maximumPads)
        return false;

    Listeners listenersCopy;
    std::optional<PadBinding> binding;
    int armedSlot = -1;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
            if (const auto* currentBinding = current->padBinding (pad))
                binding = *currentBinding;

        if (pressed && binding.has_value())
        {
            activePad.store (pad, std::memory_order_release);
            armedSlot = binding->slot;
        }
    }

    if (pressed && binding.has_value() && listenersCopy.armSlot)
        listenersCopy.armSlot (armedSlot);

    if (listenersCopy.padEvent)
        listenersCopy.padEvent (pad, binding.has_value() ? &*binding : nullptr, pressed);

    return pressed && binding.has_value();
}

bool Mapping::handleEncoder (int encoder, int value)
{
    if (encoder < 0 || encoder >= SetMapping::maximumEncoders)
        return false;

    Listeners listenersCopy;
    MacroChange change;
    bool mapped = false;

    {
        const std::lock_guard<std::mutex> lock (mutex);
        listenersCopy = listeners;

        if (current != nullptr)
        {
            if (const auto* binding = current->encoderBinding (encoder))
            {
                if (const auto* macro = current->macroFor (*binding))
                {
                    change.macro = *macro;
                    mapped = true;

                    const int pad = activePad.load (std::memory_order_relaxed);

                    if (pad >= 0)
                        if (const auto* padBinding = current->padBinding (pad))
                            change.slot = padBinding->slot;
                }
            }
        }
    }

    if (! mapped)
        return false;

    change.rawValue = std::clamp (value, 0, 127);
    change.normalized = static_cast<float> (change.rawValue) / 127.0f;
    change.value = change.macro.minimum
                   + change.normalized * (change.macro.maximum - change.macro.minimum);

    if (listenersCopy.macroEvent)
        listenersCopy.macroEvent (change);

    return true;
}

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

std::filesystem::path Mapping::findDefaultSetFile()
{
    if (const char* environment = std::getenv ("INSTRUMENT_HUB_SET"))
    {
        const std::filesystem::path fromEnvironment (environment);

        if (! fromEnvironment.empty())
            return fromEnvironment;
    }

    std::error_code ec;
    const auto directory = std::filesystem::current_path (ec);

    if (ec)
        return {};

    const std::filesystem::path candidates[] {
        directory / "set.yaml",
        directory / "shared" / "set.yaml"
    };

    for (const auto& candidate : candidates)
        if (std::filesystem::exists (candidate, ec))
            return candidate;

    return {};
}

} // namespace Controller
