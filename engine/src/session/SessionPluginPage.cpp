#include "Session.h"

#include "plugins/PageHost.h"
#include "plugins/PageHostSlot.h"
#include "plugins/SandboxHost.h"

#include <algorithm>
#include <iostream>
#include <utility>

namespace ihub {
namespace {
bool sameIdentity(const session::PluginStateIdentity& left,
                  const session::PluginStateIdentity& right) {
    return left.pluginPath == right.pluginPath && left.pluginId == right.pluginId &&
           left.pluginName == right.pluginName;
}

void notifyPage(const std::function<void()>& observer) {
    if (observer == nullptr) return;

    try {
        observer();
    } catch (...) {
    }
}
}  // namespace

std::shared_ptr<plugins::PageHost> Session::getPageHost() const {
    const std::lock_guard<std::mutex> lock(pageHostMutex);
    return pageHost;
}

std::vector<Api::PluginPageStatus> Session::getPluginPageStatuses() const {
    std::vector<Api::PluginPageStatus> result;

    auto host = getPageHost();

    if (host == nullptr || host->getStatus() == plugins::PageHost::Status::stopped) return result;

    Api::PluginPageStatus status;
    status.id = host->getPageId().toStdString();
    status.status = plugins::PageHost::toString(host->getStatus());
    status.slots = host->getSlotIndices();
    status.activeSlot = host->getActiveSlot();
    status.error = host->getFailureReason().toStdString();

    result.push_back(std::move(status));
    return result;
}

bool Session::restartPluginPage(const std::string& pageId, std::string& error) {
    error.clear();

    auto host = getPageHost();

    if (host == nullptr || host->getPageId() != juce::String(pageId)) {
        error = "no plugin page named '" + pageId + "'";
        return false;
    }

    juce::String restartError;

    if (!host->restartAndWait(360000, restartError)) {
        error = restartError.toStdString();
        return false;
    }

    retryPageWarmSlots();
    return true;
}

void Session::retryPageWarmSlots() {
    auto host = getPageHost();
    auto* pool = warmPool.getPool();

    if (host == nullptr || pool == nullptr) return;

    for (const int slot : host->getSlotIndices()) {
        if (getSandboxSlotControl(slot) != nullptr) continue;

        const auto request = pool->getRequest(slot);

        if (request.isValid()) pool->setRequest(slot, request);
    }

    (void)warmPool.preloadAll();
}

void Session::setPageStatusObserver(PageStatusObserver observer) {
    const std::lock_guard<std::mutex> lock(pageStatusObserverMutex);
    pageStatusObserver = std::move(observer);
}

std::shared_ptr<plugins::PageHost> Session::ensurePageHost(juce::String& error) {
    error.clear();

    std::lock_guard<std::mutex> startLock(pageStartMutex);

    {
        const std::lock_guard<std::mutex> lock(pageHostMutex);

        if (pageHost != nullptr) {
            if (pageHost->getStatus() == plugins::PageHost::Status::failed) {
                error = pageHost->getFailureReason();
                return nullptr;
            }

            if (pageHost->getStatus() != plugins::PageHost::Status::stopped) return pageHost;
        }
    }

    resetPageHost();

    std::vector<plugins::PageSlotDescriptor> descriptors;

    {
        const std::lock_guard<std::mutex> lock(sourceMutex);

        for (std::size_t index = 0; index < sourceConfigs.size(); ++index) {
            const auto& source = sourceConfigs[index];

            if (source.kind != SessionSlotKind::plugin) continue;

            if (source.path.isEmpty()) {
                error = "slot " + juce::String(static_cast<int>(index)) +
                        " is a plugin slot but has no bundle path";
                return nullptr;
            }

            plugins::PageSlotDescriptor descriptor;
            descriptor.slotIndex = static_cast<int>(index);
            descriptor.pluginBundle = juce::File(source.path);
            descriptors.push_back(std::move(descriptor));
        }
    }

    if (descriptors.empty()) {
        error = "the instrument page has no plugin slots";
        return nullptr;
    }

    plugins::PageHost::Options options;
    options.pageId = "tracks";
    options.slots = std::move(descriptors);
    options.sampleRate = config.device.sampleRate;
    options.maxBlockSize = config.device.bufferSize;
    options.numChannels = juce::jlimit(1, 2, config.device.numOutputChannels);
    options.startupTimeoutMs = 120000;
    options.hangTimeoutMs = 5000;
    options.controlTimeoutMs = 3000;
    options.recoveryStateTimeoutMs = 15000;
    options.recoveryAttempts = 3;

    if (loadAbort != nullptr) {
        const auto abort = loadAbort;
        options.shouldAbortStart = [abort] { return abort->load(std::memory_order_relaxed); };
    }

    auto host = plugins::PageHost::create(std::move(options));
    configurePageCallbacks(*host);

    {
        const std::lock_guard<std::mutex> lock(pageHostMutex);
        pageHost = host;
    }

    if (!host->start(error)) {
        PageStatusObserver observer;

        {
            const std::lock_guard<std::mutex> lock(pageStatusObserverMutex);
            observer = pageStatusObserver;
        }

        notifyPage(observer);
        return nullptr;
    }

    {
        PageStatusObserver observer;

        {
            const std::lock_guard<std::mutex> lock(pageStatusObserverMutex);
            observer = pageStatusObserver;
        }

        notifyPage(observer);
    }

    return host;
}

void Session::resetPageHost() noexcept {
    std::shared_ptr<plugins::PageHost> host;

    {
        const std::lock_guard<std::mutex> lock(pageHostMutex);
        host = std::move(pageHost);
    }

    if (host != nullptr) host->shutdown();

    PageStatusObserver observer;

    {
        const std::lock_guard<std::mutex> lock(pageStatusObserverMutex);
        observer = pageStatusObserver;
    }

    notifyPage(observer);
}

void Session::configurePageCallbacks(plugins::PageHost& host) {
    host.setStatusCallback([this](plugins::PageHost::Status status, const juce::String& reason) {
        onPageStatusChanged(static_cast<int>(status), reason);
    });

    for (const int slot : host.getSlotIndices()) {
        session::PluginStateIdentity identity;

        if (!pluginIdentityForSlot(slot, identity)) continue;

        host.setSlotStateCallback(
            slot, [this, slot, identity](int callbackSlot, const juce::MemoryBlock& state,
                                         std::uint64_t) {
                onPageSlotStateChanged(callbackSlot, state, identity);
            });
    }

    host.setRecoveredCallback([this](int activeSlot) { onPageRecovered(activeSlot); });

    host.setActiveSlot(warmPool.getRequestedSlot());
}

void Session::onPageSlotStateChanged(int slot, const juce::MemoryBlock& state,
                                     const session::PluginStateIdentity& identity) {
    session::PluginStateIdentity current;

    if (!pluginIdentityForSlot(slot, current) || !sameIdentity(current, identity)) return;

    if (pluginState.storeState(slot, current, state)) notifySlotStateChanged(slot);
}

void Session::onPageStatusChanged(int status, const juce::String& reason) {
    auto host = getPageHost();

    if (host != nullptr) {
        for (const int slot : host->getSlotIndices()) {
            if (status == static_cast<int>(plugins::PageHost::Status::ready)) clearSlotError(slot);

            notifySlotStateChanged(slot);
        }
    }

    const auto message =
        "session: plugin page status " +
        juce::String(plugins::PageHost::toString(static_cast<plugins::PageHost::Status>(status))) +
        (reason.isNotEmpty() ? ": " + reason : juce::String());

    juce::Logger::writeToLog(message);
    std::cerr << message << std::endl;

    PageStatusObserver observer;

    {
        const std::lock_guard<std::mutex> lock(pageStatusObserverMutex);
        observer = pageStatusObserver;
    }

    notifyPage(observer);
}

void Session::onPageRecovered(int activeSlot) {
    if (activeSlot < 0) return;

    if (warmPool.getRequestedSlot() == activeSlot) warmPool.arm(activeSlot);

    notifySlotStateChanged(activeSlot);
}

}  // namespace ihub
