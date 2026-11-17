#include "audio/WarmPool.h"

#include <chrono>
#include <utility>

namespace AudioEngine {

bool WarmPool::shutdown(int timeoutMs) noexcept {
    std::vector<std::unique_ptr<Instance>> leftovers;
    DestroyHook hook;

    {
        std::unique_lock<std::mutex> lock(mutex);

        if (shutdownComplete) return true;

        quit = true;
        warmQueue.clear();
        retryDeferred = false;

        for (auto& slot : slots) {
            slot.queued = false;
            slot.deferred = false;
        }

        condition.notify_all();

        if (worker.joinable() && !workerDone)
            workerDoneCondition.wait_for(lock, std::chrono::milliseconds(juce::jmax(0, timeoutMs)),
                                         [this] { return workerDone; });

        if (!workerDone) return false;

        if (worker.joinable()) {
            lock.unlock();
            worker.join();
            lock.lock();
        }

        hook = destroyHook;

        for (auto& slot : slots) {
            if (slot.instance != nullptr) {
                leftovers.push_back(std::move(slot.instance));
                slot.pins = 0;
            }
        }

        for (auto& instance : pendingDestroy) leftovers.push_back(std::move(instance));

        pendingDestroy.clear();
        shutdownComplete = true;
    }

    for (auto& instance : leftovers) {
        if (hook == nullptr) continue;

        try {
            hook(*instance);
        } catch (...) {
        }
    }

    leftovers.clear();
    return true;
}

WarmPool::~WarmPool() {
    if (shutdown(2000)) return;

    if (worker.joinable()) worker.join();

    (void)shutdown(0);
}

}  // namespace AudioEngine
