#include "RtGuard.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#define IHUB_RT_HAS_WRITE 1
#else
#define IHUB_RT_HAS_WRITE 0
#endif

#if IHUB_RT_GUARD
#include <juce_core/juce_core.h>

#if JUCE_ENABLE_ALLOCATION_HOOKS
#error "IHUB_RT_GUARD replaces operator new; build with JUCE_ENABLE_ALLOCATION_HOOKS=0"
#endif
#endif

namespace AudioEngine {

namespace {
std::atomic<bool> enabled{true};
std::atomic<bool> strict{false};
std::atomic<std::uint64_t> allocations{0};
std::atomic<std::uint64_t> locks{0};
std::atomic<std::uint64_t> logs{0};
std::atomic<std::uint64_t> scopeErrors{0};

void writeFatalText(const char* text) noexcept {
    if (text == nullptr) return;

#if IHUB_RT_HAS_WRITE
    std::size_t length = 0;

    while (text[length] != '\0') ++length;

    const auto written = ::write(STDERR_FILENO, text, length);
    (void)written;
#else
    std::fputs(text, stderr);
#endif
}
}  // namespace

bool RtGuard::isEnabled() noexcept { return enabled.load(std::memory_order_relaxed); }

void RtGuard::setEnabled(bool shouldBeEnabled) noexcept {
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

bool RtGuard::isStrict() noexcept { return strict.load(std::memory_order_relaxed); }

void RtGuard::setStrict(bool shouldBeStrict) noexcept {
    strict.store(shouldBeStrict, std::memory_order_relaxed);
}

int RtGuard::getOpenScopeCount() noexcept {
    return RtDetail::openScopes.load(std::memory_order_relaxed);
}

bool RtGuard::assertNoOpenScopes() noexcept {
    const int open = getOpenScopeCount();

    if (open != 0) noteViolation(Violation::scope, "a Scope outlived its render pass");

    return open == 0;
}

void RtGuard::noteAllocation(const char* what) noexcept {
#if IHUB_RT_GUARD
    if (RtDetail::audioScopeDepth <= 0) return;

    noteViolation(Violation::allocation, what);
#else
    (void)what;
#endif
}

void RtGuard::noteLock(const char* what) noexcept {
#if IHUB_RT_GUARD
    if (RtDetail::audioScopeDepth <= 0) return;

    noteViolation(Violation::lock, what);
#else
    (void)what;
#endif
}

void RtGuard::noteLog() noexcept {
#if IHUB_RT_GUARD
    if (RtDetail::audioScopeDepth <= 0) return;

    noteViolation(Violation::log, "juce::Logger");
#endif
}

void RtGuard::noteViolation(Violation kind, const char* detail) noexcept {
#if IHUB_RT_GUARD
    if (!isEnabled() || RtDetail::handlingViolation) return;

    RtDetail::handlingViolation = true;

    switch (kind) {
        case Violation::allocation:
            allocations.fetch_add(1, std::memory_order_relaxed);
            break;
        case Violation::lock:
            locks.fetch_add(1, std::memory_order_relaxed);
            break;
        case Violation::log:
            logs.fetch_add(1, std::memory_order_relaxed);
            break;
        case Violation::scope:
            scopeErrors.fetch_add(1, std::memory_order_relaxed);
            break;
    }

    if (isStrict()) fail(kind, detail);

    RtDetail::handlingViolation = false;
#else
    (void)kind;
    (void)detail;
#endif
}

void RtGuard::fail(Violation kind, const char* detail) noexcept {
    writeFatalText("rt-guard: FATAL ");
    writeFatalText(toString(kind));
    writeFatalText(" inside audio callback");

    if (detail != nullptr) {
        writeFatalText(": ");
        writeFatalText(detail);
    }

    writeFatalText("\n");
    std::abort();
}

RtGuard::Counters RtGuard::getCounters() noexcept {
    Counters result;
    result.allocations = allocations.load(std::memory_order_relaxed);
    result.locks = locks.load(std::memory_order_relaxed);
    result.logs = logs.load(std::memory_order_relaxed);
    result.scopeErrors = scopeErrors.load(std::memory_order_relaxed);
    return result;
}

std::uint64_t RtGuard::getViolationCount() noexcept { return getCounters().total(); }

void RtGuard::resetCounters() noexcept {
    allocations.store(0, std::memory_order_relaxed);
    locks.store(0, std::memory_order_relaxed);
    logs.store(0, std::memory_order_relaxed);
    scopeErrors.store(0, std::memory_order_relaxed);
}

const char* RtGuard::toString(Violation kind) noexcept {
    switch (kind) {
        case Violation::allocation:
            return "allocation";
        case Violation::lock:
            return "lock";
        case Violation::log:
            return "log";
        case Violation::scope:
            return "scope";
    }

    return "violation";
}

#if IHUB_RT_GUARD
namespace {

class RtGuardLogProbe final : public juce::Logger {
public:
    void logMessage(const juce::String& message) override {
        AudioEngine::RtGuard::noteLog();

        std::fputs(message.toRawUTF8(), stderr);
        std::fputc('\n', stderr);
    }
};

RtGuardLogProbe& getLogProbe() noexcept {
    static auto* probe = new RtGuardLogProbe();
    return *probe;
}

juce::Logger* previousLogger = nullptr;
bool logProbeInstalled = false;
}  // namespace

void RtGuard::installLogProbe() noexcept {
    if (logProbeInstalled) return;

    previousLogger = juce::Logger::getCurrentLogger();
    juce::Logger::setCurrentLogger(&getLogProbe());
    logProbeInstalled = true;
}

void RtGuard::uninstallLogProbe() noexcept {
    if (!logProbeInstalled) return;

    if (juce::Logger::getCurrentLogger() == &getLogProbe())
        juce::Logger::setCurrentLogger(previousLogger);

    previousLogger = nullptr;
    logProbeInstalled = false;
}

bool RtGuard::isLogProbeInstalled() noexcept { return logProbeInstalled; }
#else
void RtGuard::installLogProbe() noexcept {}
void RtGuard::uninstallLogProbe() noexcept {}
bool RtGuard::isLogProbeInstalled() noexcept { return false; }
#endif

}  // namespace AudioEngine
