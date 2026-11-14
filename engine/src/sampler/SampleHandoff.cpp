#include "SampleHandoff.h"

namespace ihub::sampler
{

    SampleHandoff::~SampleHandoff ()
    {
        delete installed.exchange (nullptr, std::memory_order_acq_rel);
        delete incoming.exchange (nullptr, std::memory_order_acq_rel);
        collectGarbage ();
    }

    void SampleHandoff::publish (std::unique_ptr<SampleData> sample) noexcept
    {
        auto* raw = sample.release ();

        delete incoming.exchange (raw, std::memory_order_acq_rel);
    }

    const SampleData* SampleHandoff::poll () noexcept
    {
        auto* next = incoming.exchange (nullptr, std::memory_order_acquire);

        if (next == nullptr)
            return nullptr;

        auto* previous = installed.exchange (next, std::memory_order_acq_rel);

        if (previous != nullptr)
            pushRetired (previous);

        return next;
    }

    void SampleHandoff::pushRetired (const SampleData* sample) noexcept
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        retireFifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            retireSlots[start1] = sample;
            retireFifo.finishedWrite (1);
        }
        else if (size2 > 0)
        {
            retireSlots[start2] = sample;
            retireFifo.finishedWrite (1);
        }
        else
        {
            overflowCount.fetch_add (1, std::memory_order_relaxed);
        }
    }

    void SampleHandoff::collectGarbage () noexcept
    {
        const std::lock_guard<std::mutex> lock (collectMutex);

        for (;;)
        {
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            retireFifo.prepareToRead (1, start1, size1, start2, size2);

            if (size1 <= 0 && size2 <= 0)
                break;

            const auto index = size1 > 0 ? start1 : start2;
            const auto* sample = retireSlots[(size_t)index];
            retireSlots[(size_t)index] = nullptr;
            retireFifo.finishedRead (1);

            delete sample;
        }
    }

    int SampleHandoff::getNumRetired () const noexcept
    {
        return retireFifo.getNumReady ();
    }

}
