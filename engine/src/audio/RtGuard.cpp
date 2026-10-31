#include "RtGuard.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

#if defined(__unix__) || defined(__APPLE__)
 #include <unistd.h>
 #define IHUB_RT_HAS_WRITE 1
#else
 #define IHUB_RT_HAS_WRITE 0
#endif

#if IHUB_RT_GUARD
 #include <juce_core/juce_core.h>

 // RtGuard installs a global replacement for operator new, so JUCE's own
 // allocation hooks must stay off: they define the same symbols.
 #if JUCE_ENABLE_ALLOCATION_HOOKS
  #error "IHUB_RT_GUARD replaces operator new; build with JUCE_ENABLE_ALLOCATION_HOOKS=0"
 #endif
#endif

#if IHUB_RT_GUARD && IHUB_RT_GUARD_WRAP_MALLOC \
    && defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
 #define IHUB_RT_HAS_MALLOC_WRAP 1
 #include <malloc.h>
#else
 #define IHUB_RT_HAS_MALLOC_WRAP 0
#endif

#if IHUB_RT_GUARD && IHUB_RT_GUARD_WRAP_LOCKS \
    && defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
 #define IHUB_RT_HAS_LOCK_WRAP 1
 #include <pthread.h>
#else
 #define IHUB_RT_HAS_LOCK_WRAP 0
#endif

namespace AudioEngine
{

namespace
{
std::atomic<bool> enabled { true };
std::atomic<bool> strict { false };
std::atomic<std::uint64_t> allocations { 0 };
std::atomic<std::uint64_t> locks { 0 };
std::atomic<std::uint64_t> logs { 0 };
std::atomic<std::uint64_t> scopeErrors { 0 };

/** Writes a fatal diagnostic without stdio, allocation or a lock.

    stdio is deliberately avoided: the violation might have happened while the
    callback held the stdio lock (for example a printf that allocated), and
    taking that lock again before aborting would hang instead of failing.
*/
void writeFatalText (const char* text) noexcept
{
    if (text == nullptr)
        return;

   #if IHUB_RT_HAS_WRITE
    std::size_t length = 0;

    while (text[length] != '\0')
        ++length;

    const auto written = ::write (STDERR_FILENO, text, length);
    (void) written;
   #else
    std::fputs (text, stderr);
   #endif
}
} // namespace

//==============================================================================
bool RtGuard::isEnabled() noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void RtGuard::setEnabled (bool shouldBeEnabled) noexcept
{
    enabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

bool RtGuard::isStrict() noexcept
{
    return strict.load (std::memory_order_relaxed);
}

void RtGuard::setStrict (bool shouldBeStrict) noexcept
{
    strict.store (shouldBeStrict, std::memory_order_relaxed);
}

//==============================================================================
int RtGuard::getOpenScopeCount() noexcept
{
    return RtDetail::openScopes.load (std::memory_order_relaxed);
}

bool RtGuard::assertNoOpenScopes() noexcept
{
    const int open = getOpenScopeCount();

    if (open != 0)
        noteViolation (Violation::scope, "a Scope outlived its render pass");

    return open == 0;
}

//==============================================================================
void RtGuard::noteAllocation (const char* what) noexcept
{
   #if IHUB_RT_GUARD
    if (RtDetail::audioScopeDepth <= 0)
        return;

    noteViolation (Violation::allocation, what);
   #else
    (void) what;
   #endif
}

void RtGuard::noteLock (const char* what) noexcept
{
   #if IHUB_RT_GUARD
    if (RtDetail::audioScopeDepth <= 0)
        return;

    noteViolation (Violation::lock, what);
   #else
    (void) what;
   #endif
}

void RtGuard::noteLog() noexcept
{
   #if IHUB_RT_GUARD
    if (RtDetail::audioScopeDepth <= 0)
        return;

    noteViolation (Violation::log, "juce::Logger");
   #endif
}

void RtGuard::noteViolation (Violation kind, const char* detail) noexcept
{
   #if IHUB_RT_GUARD
    if (! isEnabled() || RtDetail::handlingViolation)
        return;

    RtDetail::handlingViolation = true;

    switch (kind)
    {
        case Violation::allocation:
            allocations.fetch_add (1, std::memory_order_relaxed);
            break;
        case Violation::lock:
            locks.fetch_add (1, std::memory_order_relaxed);
            break;
        case Violation::log:
            logs.fetch_add (1, std::memory_order_relaxed);
            break;
        case Violation::scope:
            scopeErrors.fetch_add (1, std::memory_order_relaxed);
            break;
    }

    if (isStrict())
        fail (kind, detail);

    RtDetail::handlingViolation = false;
   #else
    (void) kind;
    (void) detail;
   #endif
}

void RtGuard::fail (Violation kind, const char* detail) noexcept
{
    // Fatal path only. No stdio, no allocation, no locks: see writeFatalText.
    // RtDetail::handlingViolation stays set for the whole call so the abort
    // path cannot recurse into the allocator hook.
    writeFatalText ("rt-guard: FATAL ");
    writeFatalText (toString (kind));
    writeFatalText (" inside audio callback");

    if (detail != nullptr)
    {
        writeFatalText (": ");
        writeFatalText (detail);
    }

    writeFatalText ("\n");
    std::abort();
}

//==============================================================================
RtGuard::Counters RtGuard::getCounters() noexcept
{
    Counters result;
    result.allocations = allocations.load (std::memory_order_relaxed);
    result.locks = locks.load (std::memory_order_relaxed);
    result.logs = logs.load (std::memory_order_relaxed);
    result.scopeErrors = scopeErrors.load (std::memory_order_relaxed);
    return result;
}

std::uint64_t RtGuard::getViolationCount() noexcept
{
    return getCounters().total();
}

void RtGuard::resetCounters() noexcept
{
    allocations.store (0, std::memory_order_relaxed);
    locks.store (0, std::memory_order_relaxed);
    logs.store (0, std::memory_order_relaxed);
    scopeErrors.store (0, std::memory_order_relaxed);
}

const char* RtGuard::toString (Violation kind) noexcept
{
    switch (kind)
    {
        case Violation::allocation: return "allocation";
        case Violation::lock:       return "lock";
        case Violation::log:        return "log";
        case Violation::scope:      return "scope";
    }

    return "violation";
}

//==============================================================================
#if IHUB_RT_GUARD
namespace
{
/** A Logger that reports writes made inside a realtime scope.

    Messages that are not violations are forwarded to stderr directly rather
    than through Logger, so reporting can never re-enter the probe.
*/
class RtGuardLogProbe final : public juce::Logger
{
public:
    void logMessage (const juce::String& message) override
    {
        AudioEngine::RtGuard::noteLog();

        std::fputs (message.toRawUTF8(), stderr);
        std::fputc ('\n', stderr);
    }
};

RtGuardLogProbe& getLogProbe() noexcept
{
    // Intentionally leaked: its destructor must never run while JUCE still
    // points currentLogger at it.
    static auto* probe = new RtGuardLogProbe();
    return *probe;
}

juce::Logger* previousLogger = nullptr;
bool logProbeInstalled = false;
} // namespace

void RtGuard::installLogProbe() noexcept
{
    if (logProbeInstalled)
        return;

    previousLogger = juce::Logger::getCurrentLogger();
    juce::Logger::setCurrentLogger (&getLogProbe());
    logProbeInstalled = true;
}

void RtGuard::uninstallLogProbe() noexcept
{
    if (! logProbeInstalled)
        return;

    if (juce::Logger::getCurrentLogger() == &getLogProbe())
        juce::Logger::setCurrentLogger (previousLogger);

    previousLogger = nullptr;
    logProbeInstalled = false;
}

bool RtGuard::isLogProbeInstalled() noexcept
{
    return logProbeInstalled;
}
#else
void RtGuard::installLogProbe() noexcept {}
void RtGuard::uninstallLogProbe() noexcept {}
bool RtGuard::isLogProbeInstalled() noexcept { return false; }
#endif

} // namespace AudioEngine

//==============================================================================
// Replacement allocator
//
// Only compiled when the guard is on. Outside a Scope the hooks are a
// thread-local load, so the rest of the application (UI, device manager)
// is unaffected. Inside a Scope they record a violation, and strict mode
// aborts before returning the memory.

#if IHUB_RT_GUARD

namespace
{
void* rawAllocate (std::size_t size) noexcept
{
    AudioEngine::RtDetail::skipMallocHook = true;
    void* pointer = std::malloc (size == 0 ? 1 : size);
    AudioEngine::RtDetail::skipMallocHook = false;
    return pointer;
}

void* rawAlignedAllocate (std::size_t size, std::size_t alignment) noexcept
{
    const std::size_t rounded = size == 0 ? alignment
                                          : ((size + alignment - 1) / alignment) * alignment;
    return std::aligned_alloc (alignment, rounded);
}
} // namespace

void* operator new (std::size_t size)
{
    AudioEngine::RtGuard::noteAllocation ("operator new");

    if (void* pointer = rawAllocate (size))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    AudioEngine::RtGuard::noteAllocation ("operator new (nothrow)");
    return rawAllocate (size);
}

void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    return ::operator new (size, std::nothrow);
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    AudioEngine::RtGuard::noteAllocation ("operator new (aligned)");

    if (void* pointer = rawAlignedAllocate (size, static_cast<std::size_t> (alignment)))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void* operator new (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    AudioEngine::RtGuard::noteAllocation ("operator new (aligned, nothrow)");
    return rawAlignedAllocate (size, static_cast<std::size_t> (alignment));
}

void* operator new[] (std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return ::operator new (size, alignment, std::nothrow);
}

void operator delete (void* pointer) noexcept
{
    AudioEngine::RtGuard::noteAllocation ("operator delete");
    std::free (pointer);
}

void operator delete[] (void* pointer) noexcept
{
    ::operator delete (pointer);
}

void operator delete (void* pointer, std::size_t) noexcept
{
    ::operator delete (pointer);
}

void operator delete[] (void* pointer, std::size_t) noexcept
{
    ::operator delete (pointer);
}

void operator delete (void* pointer, std::align_val_t) noexcept
{
    ::operator delete (pointer);
}

void operator delete[] (void* pointer, std::align_val_t) noexcept
{
    ::operator delete (pointer);
}

void operator delete (void* pointer, std::size_t, std::align_val_t) noexcept
{
    ::operator delete (pointer);
}

void operator delete[] (void* pointer, std::size_t, std::align_val_t) noexcept
{
    ::operator delete (pointer);
}

#endif // IHUB_RT_GUARD

//==============================================================================
// malloc wrap
//
// Linker-level interposition catches raw malloc from engine code as well as
// C++ allocations that reach the C heap. It is enabled by CMake with
// -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc.

#if IHUB_RT_HAS_MALLOC_WRAP

extern "C"
{
    void* __real_malloc (std::size_t);
    void* __wrap_malloc (std::size_t size)
    {
        if (! AudioEngine::RtDetail::skipMallocHook)
            AudioEngine::RtGuard::noteAllocation ("malloc");

        return __real_malloc (size);
    }

    void* __real_calloc (std::size_t count, std::size_t size);
    void* __wrap_calloc (std::size_t count, std::size_t size)
    {
        if (! AudioEngine::RtDetail::skipMallocHook)
            AudioEngine::RtGuard::noteAllocation ("calloc");

        return __real_calloc (count, size);
    }

    void* __real_realloc (void* pointer, std::size_t size);
    void* __wrap_realloc (void* pointer, std::size_t size)
    {
        if (! AudioEngine::RtDetail::skipMallocHook)
            AudioEngine::RtGuard::noteAllocation ("realloc");

        return __real_realloc (pointer, size);
    }
}

#endif // IHUB_RT_HAS_MALLOC_WRAP

//==============================================================================
// pthread wrappers
//
// Covers std::mutex, std::timed_mutex, std::shared_mutex, juce::CriticalSection
// and raw pthread locks, all of which are realtime hazards inside a callback.
// Enabled by CMake with -Wl,--wrap=pthread_mutex_lock,... on Linux.

#if IHUB_RT_HAS_LOCK_WRAP

extern "C"
{
    int __real_pthread_mutex_lock (pthread_mutex_t*);
    int __wrap_pthread_mutex_lock (pthread_mutex_t* mutex)
    {
        AudioEngine::RtGuard::noteLock ("pthread_mutex_lock");
        return __real_pthread_mutex_lock (mutex);
    }

    int __real_pthread_mutex_trylock (pthread_mutex_t*);
    int __wrap_pthread_mutex_trylock (pthread_mutex_t* mutex)
    {
        AudioEngine::RtGuard::noteLock ("pthread_mutex_trylock");
        return __real_pthread_mutex_trylock (mutex);
    }

    int __real_pthread_mutex_timedlock (pthread_mutex_t*, const struct timespec*);
    int __wrap_pthread_mutex_timedlock (pthread_mutex_t* mutex, const struct timespec* timeout)
    {
        AudioEngine::RtGuard::noteLock ("pthread_mutex_timedlock");
        return __real_pthread_mutex_timedlock (mutex, timeout);
    }

    int __real_pthread_spin_lock (pthread_spinlock_t*);
    int __wrap_pthread_spin_lock (pthread_spinlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_spin_lock");
        return __real_pthread_spin_lock (lock);
    }

    int __real_pthread_spin_trylock (pthread_spinlock_t*);
    int __wrap_pthread_spin_trylock (pthread_spinlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_spin_trylock");
        return __real_pthread_spin_trylock (lock);
    }

    int __real_pthread_rwlock_rdlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_rdlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_rdlock");
        return __real_pthread_rwlock_rdlock (lock);
    }

    int __real_pthread_rwlock_wrlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_wrlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_wrlock");
        return __real_pthread_rwlock_wrlock (lock);
    }

    int __real_pthread_rwlock_tryrdlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_tryrdlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_tryrdlock");
        return __real_pthread_rwlock_tryrdlock (lock);
    }

    int __real_pthread_rwlock_trywrlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_trywrlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_trywrlock");
        return __real_pthread_rwlock_trywrlock (lock);
    }
}

#endif // IHUB_RT_HAS_LOCK_WRAP
