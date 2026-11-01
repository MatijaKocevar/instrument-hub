#pragma once

#include "SampleData.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace ihub::sampler
{

/**
    A lock-free single-slot hand-off of one fully prepared `SampleData`.

    The producer (the loader's background thread, or any non-audio thread)
    decodes, resamples and allocates the sample completely before publishing it.
    The audio thread then exchanges a raw pointer. Nothing on the audio path
    allocates, locks, logs or frees memory.

    ## Contract

    - `publish()` may be called from one producer thread. If a previous sample
      was published but not consumed yet, it is destroyed immediately on the
      producer thread (the audio thread never saw it).
    - `poll()` and `current()` are the **audio-thread** calls. They are
      allocation-free and lock-free.
    - `poll()` installs a newly published sample and moves the previously
      installed one to the retirement queue. The returned pointer is borrowed.
      Because replacement retires the old buffer, only poll a new sample when
      no voice can still be reading the old one (an idle voice pool, or a
      sample boundary chosen by the instrument). The loader never replaces a
      sample behind a playing voice.
    - `collectGarbage()` destroys retired samples. Call it only from a
      non-audio thread **and only while the audio thread cannot be holding a
      pointer into this hand-off** (for example with the stream stopped, or
      after every voice has released). This is the standard deferred-reclaim
      pattern: the pointers handed to the audio thread remain valid until the
      owner explicitly reclaims them, so there is no use-after-free if the
      contract is followed.
    - Destroy the hand-off only after the audio callback has stopped.

    The hand-off is deliberately not reference counted: an atomic refcount
    would still need to free the last reference somewhere, and freeing on the
    audio thread is forbidden. Deferring reclamation to the owner makes the
    boundary explicit.
*/
class SampleHandoff
{
public:
    /** Number of retired samples that can be queued before the owner drains
        them. One slot is enough for ordinary use; the queue keeps several
        replacements from accumulating if the owner is slow. */
    static constexpr int retireCapacity = 64;

    SampleHandoff() = default;
    ~SampleHandoff();

    SampleHandoff (const SampleHandoff&) = delete;
    SampleHandoff& operator= (const SampleHandoff&) = delete;

    //==============================================================================
    // Producer side (never the audio thread)

    /** Takes ownership of `sample` and makes it available to `poll()`. */
    void publish (std::unique_ptr<SampleData> sample) noexcept;

    /** Destroys every retired sample. See the lifetime contract above. */
    void collectGarbage() noexcept;

    //==============================================================================
    // Audio-thread side (all allocation-free and lock-free)

    /** Installs a newly published sample, if there is one.

        Returns the newly installed sample, or nullptr when nothing new was
        published. The previous sample, if any, is moved to the retirement
        queue. The returned pointer stays valid until `collectGarbage()` runs.
    */
    const SampleData* poll() noexcept;

    /** The currently installed sample, or nullptr. */
    const SampleData* current() const noexcept
    {
        return installed.load (std::memory_order_acquire);
    }

    //==============================================================================
    // Inspection (non-audio threads)

    /** Retired samples waiting for `collectGarbage()`. */
    int getNumRetired() const noexcept;

    /** Retirements dropped because the queue was full. Must stay zero in a
        correctly paced system; a non-zero value means the owner never
        drained the queue and the sample was intentionally leaked rather than
        freed on the audio thread. */
    int getNumOverflows() const noexcept { return overflowCount.load (std::memory_order_relaxed); }

    /** Samples published but not consumed yet. 0 or 1. */
    bool hasPendingPublish() const noexcept
    {
        return incoming.load (std::memory_order_acquire) != nullptr;
    }

private:
    void pushRetired (const SampleData* sample) noexcept;

    std::atomic<const SampleData*> incoming { nullptr };
    std::atomic<const SampleData*> installed { nullptr };
    std::atomic<int> overflowCount { 0 };

    juce::AbstractFifo retireFifo { retireCapacity };
    const SampleData* retireSlots[retireCapacity] {};

    // Serialises owners that call collectGarbage() from more than one
    // non-audio thread. Never touched by the audio thread.
    std::mutex collectMutex;
};

} // namespace ihub::sampler
