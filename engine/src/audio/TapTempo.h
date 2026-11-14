#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace AudioEngine
{

    class TapTempo final
    {
    public:
        static constexpr int defaultHistorySize = 8;

        static constexpr int maximumHistorySize = 32;

        static constexpr double minBpm = 20.0;
        static constexpr double maxBpm = 300.0;

        static constexpr double defaultBpm = 120.0;

        static constexpr double minimumIntervalSeconds = 60.0 / maxBpm;

        static constexpr double maximumIntervalSeconds = 60.0 / minBpm;

        explicit TapTempo (int historySize = defaultHistorySize) noexcept;
        ~TapTempo () = default;

        TapTempo (const TapTempo&) = delete;
        TapTempo& operator= (const TapTempo&) = delete;

        double tapAt (double timestampSeconds) noexcept;

        double tap () noexcept;

        void reset () noexcept;

        double getBpm () const noexcept;

        bool hasEstimate () const noexcept;

        int getTapCount () const noexcept;

        int getIntervalCount () const noexcept;

        int getHistorySize () const noexcept;

        double getLastIntervalSeconds () const noexcept;

        static constexpr double intervalSecondsForBpm (double tempo) noexcept
        {
            return tempo > 0.0 ? 60.0 / tempo : 0.0;
        }

        static constexpr double bpmForIntervalSeconds (double seconds) noexcept
        {
            return seconds > 0.0 ? 60.0 / seconds : 0.0;
        }

    private:
        class ScopedLock
        {
        public:
            explicit ScopedLock (std::atomic_flag& target) noexcept : flag (target)
            {
                while (flag.test_and_set (std::memory_order_acquire))
                {
                }
            }

            ~ScopedLock ()
            {
                flag.clear (std::memory_order_release);
            }

            ScopedLock (const ScopedLock&) = delete;
            ScopedLock& operator= (const ScopedLock&) = delete;

        private:
            std::atomic_flag& flag;
        };

        double medianIntervalSeconds () const noexcept;

        const int historySize;

        std::atomic_flag busy = ATOMIC_FLAG_INIT;

        int taps = 0;
        double lastTapTime = 0.0;
        double intervals[maximumHistorySize]{};
        int intervalCount = 0;
        int intervalHead = 0;

        std::atomic<double> bpm{0.0};
        std::atomic<int> tapCount{0};
        std::atomic<int> publishedIntervalCount{0};
        std::atomic<double> lastIntervalSeconds{0.0};
    };

}
