#include "Session.h"

#include "plugins/PageHost.h"
#include "plugins/PageHostSlot.h"
#include "plugins/SandboxHost.h"

#include <algorithm>

namespace ihub {
namespace {
constexpr double pageSlotStartGraceMs = 2000.0;

bool sameIdentity(const session::PluginStateIdentity& left,
                  const session::PluginStateIdentity& right) {
    return left.pluginPath == right.pluginPath && left.pluginId == right.pluginId &&
           left.pluginName == right.pluginName;
}
}  // namespace

void Session::registerSandboxSlot(int slot, plugins::SandboxHost& host,
                                  const session::PluginStateIdentity& identity) {
    const std::lock_guard<std::mutex> lock(sandboxMutex);

    sandboxSlots.erase(
        std::remove_if(sandboxSlots.begin(), sandboxSlots.end(),
                       [&host](const SandboxSlot& entry) { return entry.host == &host; }),
        sandboxSlots.end());

    const auto generation = host.getGeneration();
    sandboxSlots.push_back({slot, &host, identity, generation});

    if (slot >= 0 && slot < static_cast<int>(sandboxGenerations.size()))
        sandboxGenerations[static_cast<std::size_t>(slot)] =
            juce::jmax(sandboxGenerations[static_cast<std::size_t>(slot)], generation);
}

void Session::unregisterSandboxSlot(plugins::SandboxHost& host) {
    SandboxSlot removed;

    {
        const std::lock_guard<std::mutex> lock(sandboxMutex);

        const auto found =
            std::find_if(sandboxSlots.begin(), sandboxSlots.end(),
                         [&host](const SandboxSlot& entry) { return entry.host == &host; });

        if (found == sandboxSlots.end()) return;

        removed = *found;
        sandboxSlots.erase(found);
    }

    if (shuttingDown.load(std::memory_order_acquire)) return;

    {
        const std::lock_guard<std::mutex> lock(sandboxMutex);

        if (removed.slot < 0 || removed.slot >= static_cast<int>(sandboxGenerations.size()) ||
            removed.generation != sandboxGenerations[static_cast<std::size_t>(removed.slot)])
            return;
    }

    session::PluginStateIdentity current;

    if (!pluginIdentityForSlot(removed.slot, current) || !sameIdentity(current, removed.identity))
        return;

    if (pluginState.getInstrument(removed.slot) != nullptr) {
        (void)capturePluginState(removed.slot);
        return;
    }

    juce::MemoryBlock state;

    if (host.isRunning() && !host.isFailed()) state = host.getStateInformation();

    if (state.getSize() == 0) state = host.getLastCommittedState();

    if (state.getSize() > 0) (void)pluginState.storeState(removed.slot, removed.identity, state);
}

plugins::SandboxHost* Session::sandboxForSlot(int slot) const noexcept {
    const std::lock_guard<std::mutex> lock(sandboxMutex);

    for (auto entry = sandboxSlots.rbegin(); entry != sandboxSlots.rend(); ++entry)
        if (entry->slot == slot) return entry->host;

    return nullptr;
}

plugins::SandboxHost* Session::getSandboxHost(int slot) const noexcept {
    return sandboxForSlot(slot);
}

plugins::SandboxSlotControl* Session::getSandboxSlotControl(int slot) const noexcept {
    if (auto* legacy = sandboxForSlot(slot); legacy != nullptr) return legacy;

    const std::lock_guard<std::mutex> lock(pageSlotHandleMutex);

    if (slot < 0 || slot >= static_cast<int>(pageSlotHandles.size())) return nullptr;

    return pageSlotHandles[static_cast<std::size_t>(slot)];
}

int Session::getSandboxProcessId(int slot) const noexcept {
    if (auto* host = sandboxForSlot(slot); host != nullptr) return host->getChildProcessId();

    if (auto host = getPageHost(); host != nullptr && host->hasSlot(slot))
        return host->getChildProcessId();

    return 0;
}

void Session::setSlotError(int slot, const juce::String& error) {
    const std::lock_guard<std::mutex> lock(slotErrorMutex);

    if (slot >= 0 && slot < static_cast<int>(slotErrors.size()))
        slotErrors[static_cast<std::size_t>(slot)] = error;
}

void Session::clearSlotError(int slot) { setSlotError(slot, {}); }

juce::String Session::getSlotError(int slot) const {
    {
        const std::lock_guard<std::mutex> lock(slotErrorMutex);

        if (slot >= 0 && slot < static_cast<int>(slotErrors.size()) &&
            slotErrors[static_cast<std::size_t>(slot)].isNotEmpty())
            return slotErrors[static_cast<std::size_t>(slot)];
    }

    if (auto host = getPageHost(); host != nullptr && host->hasSlot(slot))
        return host->getSlotStatus(slot).error;

    return {};
}

bool Session::isSlotPluginFailed(int slot) const noexcept {
    if (auto* host = sandboxForSlot(slot); host != nullptr) return host->isFailed();

    if (auto host = getPageHost(); host != nullptr && host->hasSlot(slot))
        return host->getSlotStatus(slot).failed;

    return false;
}

bool Session::waitForPageSlot(int slot, int timeoutMs) {
    auto* pool = warmPool.getPool();

    if (pool == nullptr) return false;

    const auto started = juce::Time::getMillisecondCounterHiRes();
    const auto deadline = started + static_cast<double>(juce::jmax(0, timeoutMs));

    for (;;) {
        if (getSandboxSlotControl(slot) != nullptr) return true;

        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (now >= deadline) return false;

        const bool loading = pool->isWarming(slot) || pool->isWarm(slot);

        if (!loading && now - started > pageSlotStartGraceMs) return false;

        juce::Thread::sleep(25);
    }
}

std::uint64_t Session::nextSandboxGeneration(int slot) {
    const std::lock_guard<std::mutex> lock(sandboxMutex);

    const auto generation = ++sandboxGenerationCounter;

    if (slot >= 0 && slot < static_cast<int>(sandboxGenerations.size()))
        sandboxGenerations[static_cast<std::size_t>(slot)] = generation;

    return generation;
}

std::uint64_t Session::bumpSandboxGeneration(int slot) { return nextSandboxGeneration(slot); }

bool Session::isCurrentSandboxGeneration(int slot, std::uint64_t generation) const noexcept {
    const std::lock_guard<std::mutex> lock(sandboxMutex);

    if (slot < 0 || slot >= static_cast<int>(sandboxGenerations.size())) return false;

    return sandboxGenerations[static_cast<std::size_t>(slot)] == generation;
}

void Session::notifySlotStateChanged(int slot) {
    session::WarmPoolController::SlotStateObserver observer;

    {
        const std::lock_guard<std::mutex> lock(slotObserverMutex);
        observer = slotStateObserver;
    }

    if (observer == nullptr) return;

    try {
        observer(slot);
    } catch (...) {
    }
}

}  // namespace ihub
