#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace ihub::sampler
{

/** One decoded and resampled sample, ready for playback at the engine rate.

    `audio` is always at `sampleRate`, which is the engine rate that was
    configured when the file was loaded. The source file's own properties are
    kept for display and for tests.

    Channels are limited to two (mono stays mono, stereo stays stereo, and
    files with more channels contribute their first two). Resampling is done
    offline with a windowed-sinc low-pass, so the buffer is band-limited to the
    engine Nyquist even when the file is at a higher rate.
*/
struct SampleData
{
    juce::AudioBuffer<float> audio;

    /** Sample rate of `audio` (the engine rate). */
    double sampleRate = 0.0;

    /** Sample rate of the file that was decoded. */
    double sourceSampleRate = 0.0;

    /** Frames in the source file. */
    juce::int64 sourceFrames = 0;

    /** Channels in the source file (before the two-channel limit). */
    int sourceChannels = 0;

    juce::String name;
    juce::String path;

    int getNumChannels() const noexcept { return audio.getNumChannels(); }
    int getNumSamples() const noexcept { return audio.getNumSamples(); }
    bool isEmpty() const noexcept { return audio.getNumSamples() == 0 || audio.getNumChannels() == 0; }

    double getLengthSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) audio.getNumSamples() / sampleRate : 0.0;
    }

    double getSourceLengthSeconds() const noexcept
    {
        return sourceSampleRate > 0.0 ? (double) sourceFrames / sourceSampleRate : 0.0;
    }
};

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

    static std::unique_ptr<SampleData> decode (const juce::File& file,
                                               double engineSampleRate,
                                               juce::String& error);

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
