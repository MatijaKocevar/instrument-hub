#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#ifndef IHUB_RT_GUARD
#define IHUB_RT_GUARD 0
#endif

namespace AudioEngine
{

    namespace RtDetail
    {
        inline thread_local int audioScopeDepth = 0;

        inline std::atomic<int> openScopes{0};

        inline thread_local bool handlingViolation = false;

        inline thread_local bool skipMallocHook = false;
    }

    class RtGuard final
    {
    public:
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

            std::uint64_t total () const noexcept
            {
                return allocations + locks + logs + scopeErrors;
            }
        };

        static constexpr bool isCompiledIn () noexcept
        {
#if IHUB_RT_GUARD
            return true;
#else
            return false;
#endif
        }

        class Scope final
        {
        public:
            Scope () noexcept
            {
#if IHUB_RT_GUARD
                ++RtDetail::audioScopeDepth;
                RtDetail::openScopes.fetch_add (1, std::memory_order_relaxed);
#endif
            }

            ~Scope () noexcept
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

        static int getOpenScopeCount () noexcept;

        static bool assertNoOpenScopes () noexcept;

        static void setEnabled (bool shouldBeEnabled) noexcept;
        static bool isEnabled () noexcept;

        static void setStrict (bool shouldBeStrict) noexcept;
        static bool isStrict () noexcept;

        static bool isInsideAudioScope () noexcept
        {
            return RtDetail::audioScopeDepth > 0;
        }

        static int getThreadScopeDepth () noexcept
        {
            return RtDetail::audioScopeDepth;
        }

        static void noteAllocation (const char* what = nullptr) noexcept;
        static void noteLock (const char* what = nullptr) noexcept;
        static void noteLog () noexcept;

        static void noteViolation (Violation kind, const char* detail = nullptr) noexcept;

        static Counters getCounters () noexcept;
        static std::uint64_t getViolationCount () noexcept;
        static void resetCounters () noexcept;

        static void installLogProbe () noexcept;
        static void uninstallLogProbe () noexcept;
        static bool isLogProbeInstalled () noexcept;

        static const char* toString (Violation kind) noexcept;

    private:
        [[noreturn]] static void fail (Violation kind, const char* detail) noexcept;
    };

}
