#include "MirrorSync.h"

#include "plugins/SandboxSlotControl.h"

#include <chrono>
#include <utility>

namespace ihub::session {

MirrorSync::MirrorSync(int slotCapacity, SandboxLookup sandboxLookup)
    : capacity(juce::jmax(0, slotCapacity)),
      lookupSandbox(std::move(sandboxLookup)),
      pending(static_cast<std::size_t>(capacity)) {
    worker = std::thread([this] { run(); });
}

MirrorSync::~MirrorSync() { shutdown(); }

void MirrorSync::parameterChanged(int slot, const juce::String& id, float normalisedValue) {
    if (slot < 0 || slot >= capacity || id.isEmpty()) return;

    {
        const std::lock_guard<std::mutex> guard(lock);

        auto& entry = pending[static_cast<std::size_t>(slot)];

        if (entry.stateDirty) {
            entry.lastEditMs = juce::Time::getMillisecondCounterHiRes();
            return;
        }

        entry.parameters[id] = juce::jlimit(0.0f, 1.0f, normalisedValue);
        entry.lastEditMs = juce::Time::getMillisecondCounterHiRes();
    }

    wake.notify_one();
}

void MirrorSync::stateChanged(int slot, juce::MemoryBlock state) {
    if (slot < 0 || slot >= capacity || state.getSize() == 0) return;

    {
        const std::lock_guard<std::mutex> guard(lock);

        auto& entry = pending[static_cast<std::size_t>(slot)];
        entry.parameters.clear();
        entry.state = std::move(state);
        entry.stateDirty = true;
        entry.lastEditMs = juce::Time::getMillisecondCounterHiRes();
    }

    wake.notify_one();
}

void MirrorSync::clear() {
    {
        const std::lock_guard<std::mutex> guard(lock);

        for (auto& entry : pending) {
            entry.parameters.clear();
            entry.state.reset();
            entry.stateDirty = false;
            entry.lastEditMs = 0.0;
        }
    }

    wake.notify_all();
}

void MirrorSync::shutdown() {
    {
        const std::lock_guard<std::mutex> guard(lock);

        if (quit) return;

        quit = true;
    }

    wake.notify_all();

    if (worker.joinable()) worker.join();
}

void MirrorSync::run() {
    std::unique_lock<std::mutex> guard(lock);

    while (!quit) {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        double waitMs = 100.0;
        int target = -1;
        bool sendState = false;

        for (int slot = 0; slot < capacity; ++slot) {
            auto& entry = pending[static_cast<std::size_t>(slot)];
            const auto idleMs = now - entry.lastEditMs;

            if (entry.stateDirty) {
                const auto remaining = stateSettleMs - idleMs;

                if (remaining <= 0.0) {
                    target = slot;
                    sendState = true;
                    entry.stateDirty = false;
                    entry.parameters.clear();
                    break;
                }

                waitMs = juce::jmin(waitMs, remaining);
                continue;
            }

            if (entry.parameters.empty()) continue;

            const auto remaining = parameterSettleMs - idleMs;

            if (remaining <= 0.0) {
                target = slot;
                break;
            }

            waitMs = juce::jmin(waitMs, remaining);
        }

        if (target < 0) {
            wake.wait_for(guard,
                          std::chrono::duration<double, std::milli>(juce::jmax(1.0, waitMs)));
            continue;
        }

        std::vector<ParameterEdit> edits;
        juce::MemoryBlock state;

        if (sendState) {
            state = std::move(pending[static_cast<std::size_t>(target)].state);
            pending[static_cast<std::size_t>(target)].state.reset();
        } else {
            auto& parameters = pending[static_cast<std::size_t>(target)].parameters;
            edits.reserve(static_cast<std::size_t>(maximumParameterBatch));

            while (!parameters.empty() && static_cast<int>(edits.size()) < maximumParameterBatch) {
                auto first = parameters.begin();
                edits.push_back({first->first, first->second});
                parameters.erase(first);
            }
        }

        guard.unlock();

        if (sendState)
            applyState(target, std::move(state));
        else
            applyParameters(target, std::move(edits));

        guard.lock();
    }
}

void MirrorSync::applyState(int slot, juce::MemoryBlock state) {
    auto* sandbox = lookupSandbox != nullptr ? lookupSandbox(slot) : nullptr;

    if (sandbox == nullptr || !sandbox->isRunning() || state.getSize() == 0) return;

    (void)sandbox->setStateInformation(state);
}

void MirrorSync::applyParameters(int slot, std::vector<ParameterEdit> edits) {
    auto* sandbox = lookupSandbox != nullptr ? lookupSandbox(slot) : nullptr;

    if (sandbox == nullptr || !sandbox->isRunning() || edits.empty()) return;

    std::vector<plugins::SandboxParameterEdit> parameterEdits;
    parameterEdits.reserve(edits.size());

    for (const auto& edit : edits) parameterEdits.push_back({edit.id, edit.normalisedValue});

    (void)sandbox->setParameters(parameterEdits);
}

}  // namespace ihub::session
