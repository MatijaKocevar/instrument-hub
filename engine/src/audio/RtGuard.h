#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

// The realtime safety harness is a development-time assertion layer. It is
// compiled in for Debug and RelWithDebInfo builds (see the top-level
// CMakeLists.txt) and compiles to an empty shell in Release, so a shipping
// binary pays nothing and links no replacement allocator.
#ifndef IHUB_RT_GUARD
 #define IHUB_RT_GUARD 0
#endif

namespace AudioEngine
{

namespace RtDetail
{
/** Per-thread nesting depth of RtGuard::Scope. Zero outside realtime code. */
inline thread_local int audioScopeDepth = 0;

/** Number of Scope objects currently open on all threads.

    This is the LeakedObjectDetector analogue: after a render pass every scope
    must have been destroyed, otherwise something escaped the callback.
*/
inline std::atomic<int> openScopes { 0 };

/** Set while the guard is already reporting a violation.
    Stops the reporting path (which may call fprintf/malloc) from recursing
    into the allocator hook.
*/
inline thread_local bool handlingViolation = false;

/** Set while operator new is inside its own malloc call, so the malloc
    wrapper does not double-count a single allocation.
*/
inline thread_local bool skipMallocHook = false;
} // namespace RtDetail

/**
    Debug-only realtime safety guard.

    `Scope` marks the current thread as executing realtime code. While a scope
    is open anywhere on the current thread:

    - every `operator new` / `operator delete` is a violation (allocation),
    - every `pthread_mutex_*`, `pthread_spin_*` and `pthread_rwlock_*` call is a
      violation (lock), on builds that wrap those symbols,
    - every `juce::Logger` write is a violation (log), while the log probe is
      installed.

    In strict mode (`--rt-strict`) the first violation writes a diagnostic to
    stderr and calls `std::abort()`, so a test that injects a `new` inside the
    callback fails with SIGABRT instead of silently passing. Non-strict mode
    only counts violations, which lets a test prove the counter is live without
    killing the process.

    The guard is deliberately allocation-free and lock-free on the happy path:
    entering a scope is a thread-local increment plus one relaxed atomic
    increment, and the hooks return after a thread-local load when no scope is
    open.

    Threading:

    - `Scope` is constructed and destroyed on the audio thread.
    - The `note*` functions may be called from any thread.
    - `setEnabled`, `setStrict`, `installLogProbe`, `resetCounters` and the
      getters are message-thread calls.

    Limitation: raw `malloc`/`calloc`/`realloc` calls are only seen when the
    target is linked with `-Wl,--wrap=malloc,...` (the CMake build does this on
    Linux for guard-enabled targets). `operator new` is always seen.
*/
class RtGuard final
{
public:
    /** The kinds of realtime violation the guard can detect. */
    enum class Violation
    {
        allocation,
        lock,
        log,
        scope
    };

    struct Counters
    {
        std::uint64_t allocations = 0;
        std::uint64_t locks = 0;
        std::uint64_t logs = 0;
        std::uint64_t scopeErrors = 0;

        std::uint64_t total() const noexcept
        {
            return allocations + locks + logs + scopeErrors;
        }
    };

    /** True when the harness is compiled into this binary. */
    static constexpr bool isCompiledIn() noexcept
    {
       #if IHUB_RT_GUARD
        return true;
       #else
        return false;
       #endif
    }

    //==============================================================================
    /** Marks the current thread as running realtime code for its lifetime.

        Construct at the top of `processBlock()` / the device callback. It is
        safe to nest; the depth unwinds in reverse order.
    */
    class Scope final
    {
    public:
        Scope() noexcept
        {
           #if IHUB_RT_GUARD
            ++RtDetail::audioScopeDepth;
            RtDetail::openScopes.fetch_add (1, std::memory_order_relaxed);
           #endif
        }

        ~Scope() noexcept
        {
           #if IHUB_RT_GUARD
            if (RtDetail::audioScopeDepth <= 0)
                RtGuard::noteViolation (Violation::scope,
                                        "Scope destroyed without a matching entry");
            else
                --RtDetail::audioScopeDepth;

            RtDetail::openScopes.fetch_sub (1, std::memory_order_relaxed);
           #endif
        }

        Scope (const Scope&) = delete;
        Scope& operator= (const Scope&) = delete;
    };

    //==============================================================================
    // LeakedObjectDetector-style bookkeeping

    /** Scope objects that are still alive across all threads. */
    static int getOpenScopeCount() noexcept;

    /** Records a scope violation if any scope leaked, and returns whether the
        count was zero. Aborts in strict mode.
    */
    static bool assertNoOpenScopes() noexcept;

    //==============================================================================
    // Control (off the audio thread)

    static void setEnabled (bool shouldBeEnabled) noexcept;
    static bool isEnabled() noexcept;

    /** When strict, the first violation aborts the process. */
    static void setStrict (bool shouldBeStrict) noexcept;
    static bool isStrict() noexcept;

    //==============================================================================
    // Audio thread

    /** True when the calling thread is inside at least one Scope. */
    static bool isInsideAudioScope() noexcept { return RtDetail::audioScopeDepth > 0; }

    static int getThreadScopeDepth() noexcept { return RtDetail::audioScopeDepth; }

    /** Hooks called by the replacement allocator, the pthread wrappers and the
        log probe. Each is a no-op outside a scope, or when the guard is off.
    */
    static void noteAllocation (const char* what = nullptr) noexcept;
    static void noteLock (const char* what = nullptr) noexcept;
    static void noteLog() noexcept;

    /** Records one violation and, in strict mode, aborts. */
    static void noteViolation (Violation kind, const char* detail = nullptr) noexcept;

    //==============================================================================
    // Inspection (any thread)

    static Counters getCounters() noexcept;
    static std::uint64_t getViolationCount() noexcept;
    static void resetCounters() noexcept;

    //==============================================================================
    // Logging probe

    /** Routes juce::Logger output through the guard until uninstalled.

        Install this before rendering in strict mode, otherwise a log call only
        shows up if it goes through one of the other hooks.
    */
    static void installLogProbe() noexcept;
    static void uninstallLogProbe() noexcept;
    static bool isLogProbeInstalled() noexcept;

    static const char* toString (Violation kind) noexcept;

private:
    [[noreturn]] static void fail (Violation kind, const char* detail) noexcept;
};

} // namespace AudioEngine
