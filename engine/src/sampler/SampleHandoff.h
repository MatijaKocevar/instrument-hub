#pragma once

#include "SampleData.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace ihub::sampler
{

    class SampleHandoff
    {
    public:
        static constexpr int retireCapacity = 64;

        SampleHandoff () = default;
        ~SampleHandoff ();

        SampleHandoff (const SampleHandoff&) = delete;
        SampleHandoff& operator= (const SampleHandoff&) = delete;

        void publish (std::unique_ptr<SampleData> sample) noexcept;

        void collectGarbage () noexcept;

        const SampleData* poll () noexcept;

        const SampleData* current () const noexcept
        {
            return installed.load (std::memory_order_acquire);
        }

        int getNumRetired () const noexcept;

        int getNumOverflows () const noexcept
        {
            return overflowCount.load (std::memory_order_relaxed);
        }

        bool hasPendingPublish () const noexcept
        {
            return incoming.load (std::memory_order_acquire) != nullptr;
        }

    private:
        void pushRetired (const SampleData* sample) noexcept;

        std::atomic<const SampleData*> incoming{nullptr};
        std::atomic<const SampleData*> installed{nullptr};
        std::atomic<int> overflowCount{0};

        juce::AbstractFifo retireFifo{retireCapacity};
        const SampleData* retireSlots[retireCapacity]{};

        std::mutex collectMutex;
    };

}
