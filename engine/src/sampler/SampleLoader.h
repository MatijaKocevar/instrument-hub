#pragma once

#include "SampleData.h"
#include "SampleHandoff.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

namespace ihub::sampler
{

/**
    Decodes audio files in the background and resamples them to the engine rate.

    ## Decoding

    WAV, AIFF and FLAC are registered through `juce::AudioFormatManager`.
    `loadBlocking()` does the whole job synchronously on the calling thread and
    is what a kit loader uses. `loadAsync()` queues a job for the loader's own
    background thread and publishes the result through a `SampleHandoff`, so
    neither decoding nor allocation happens on the audio thread.

    ## Resampling

    A file whose rate already equals the engine rate is copied bit for bit.
    Every other rate goes through a polyphase windowed-sinc low-pass. The
    cutoff is the lower of the two Nyquist frequencies, so downsampling is
    band-limited instead of aliasing. Output length is
    `round(sourceFrames * engineRate / sourceRate)`, which makes the duration
    of the resampled file match the source to within half a sample.

    ## Threading

    - `loadBlocking()` and `loadAsync()` may be called from any non-audio
      thread. The request queue is safe against several producers.
    - `poll()` and `current()` are audio-thread calls.
    - `takeEvent()`, `waitForIdle()` and `collectGarbage()` are for control
      threads; never call them from the audio callback.
    - A request's destination hand-off must outlive the request. The loader's
      own hand-off is available through `getHandoff()`.
*/
class SampleLoader : private juce::Thread
{
public:
    using Ptr = std::shared_ptr<const SampleData>;

    /** Completion of one background request, for the UI/control thread. */
    struct Event
    {
        int requestId = 0;
        bool success = false;
        juce::String path;
        juce::String error;
    };

    SampleLoader();
    ~SampleLoader() override;

    //==============================================================================
    // Setup

    /** Sets the rate every subsequent load is resampled to.

        Accepts 8 kHz to 384 kHz. Returns false for a non-finite or
        out-of-range rate, leaving the previous value unchanged. The default is
        48 kHz. Safe from any non-audio thread.
    */
    bool prepare (double engineSampleRate) noexcept;
    double getEngineSampleRate() const noexcept;

    //==============================================================================
    // Loading

    /** Decodes and resamples a file synchronously.

        Returns nullptr and fills `error` for a missing or unreadable file, an
        unsupported format, an empty file, or a file that exceeds the duration
        limit. Safe from any non-audio thread.
    */
    static Ptr loadBlocking (const juce::File& file,
                             double engineSampleRate,
                             juce::String& error);

    /** Queues a background load whose result is published to `destination`.

        Returns a positive request id, or 0 and fills `error` when the file is
        missing or the queue is full. The destination must outlive the request.
        Completion (success or failure) is reported through `takeEvent()`.
    */
    int loadAsync (const juce::File& file, SampleHandoff& destination, juce::String& error);

    /** Queues a background load into the loader's own hand-off. */
    int loadAsync (const juce::File& file, juce::String& error);

    //==============================================================================
    // Audio-thread hand-off

    SampleHandoff& getHandoff() noexcept { return handoff; }
    const SampleHandoff& getHandoff() const noexcept { return handoff; }

    /** See `SampleHandoff::poll()`. */
    const SampleData* poll() noexcept { return handoff.poll(); }

    /** See `SampleHandoff::current()`. */
    const SampleData* current() const noexcept { return handoff.current(); }

    /** Destroys retired samples. Only call with the audio thread stopped or
        with every voice done; see `SampleHandoff`. */
    void collectGarbage() noexcept { handoff.collectGarbage(); }

    //==============================================================================
    // Control-thread inspection

    /** A queued or in-flight background request. */
    bool isBusy() const noexcept { return pending.load (std::memory_order_acquire) > 0; }
    bool isIdle() const noexcept { return ! isBusy(); }

    /** Pops one completion event. Returns false when none is queued. */
    bool takeEvent (Event& event);

    /** Waits for every queued request to finish. Returns false on timeout.
        Never call from the loader's worker thread or the audio thread. */
    bool waitForIdle (int timeoutMs = 10000);

    /** Requests waiting or in flight. */
    int getNumPending() const noexcept { return pending.load (std::memory_order_acquire); }

    /** Events dropped because the consumer never drained them. */
    int getNumDroppedEvents() const noexcept
    {
        return droppedEvents.load (std::memory_order_relaxed);
    }

    /** Largest file accepted, by duration. Anything longer is rejected before
        allocating its buffer. */
    static constexpr double maximumDurationSeconds = 900.0;

    /** Absolute cap on source frames, independent of the duration limit. */
    static constexpr juce::int64 maximumSourceFrames = 200'000'000;

private:
    struct Job
    {
        int id = 0;
        juce::File file;
        double engineSampleRate = 48000.0;
        SampleHandoff* destination = nullptr;
    };

    static constexpr int maximumQueuedJobs = 64;
    static constexpr int eventCapacity = 32;

    void run() override;

    void pushEvent (Event event);
    void finishJob (int id, const juce::File& file, std::unique_ptr<SampleData> sample,
                    juce::String error, SampleHandoff& destination);

    std::atomic<double> engineSampleRate { 48000.0 };
    std::atomic<int> pending { 0 };
    std::atomic<int> nextRequestId { 1 };
    std::atomic<int> droppedEvents { 0 };

    std::mutex queueMutex;
    std::deque<Job> jobs;
    juce::WaitableEvent wakeUp { false };

    SampleHandoff handoff;

    juce::AbstractFifo eventFifo { eventCapacity };
    Event events[eventCapacity];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleLoader)
};

} // namespace ihub::sampler
