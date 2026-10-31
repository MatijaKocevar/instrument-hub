#pragma once

#include <atomic>
#include <cstdint>

namespace AudioEngine
{

/**
    The musical transport: tempo, playing state, PPQ position and sample
    position.

    Positions advance only when the transport is playing, and only by the
    number of frames the audio callback actually processed. Nothing here reads
    a wall clock on the audio thread, so the result is sample-accurate and
    deterministic: at 120 BPM and 48 kHz, 480 000 processed frames move the
    position by exactly 20 quarter notes.

    Units:

    - `ppq` is quarter notes since the last rewind, the same field the control
      API publishes as `/transport.ppq`.
    - `positionSamples` is the number of played engine frames since the last
      rewind. With a constant tempo the two differ only by
      `bpm / 60 / sampleRate`.
    - One internal tick is `1 / ppqTicksPerQuarterNote` of a quarter note.
      That constant only exists so callers and tests can talk about the
      resolution; positions are stored as doubles and are not quantised.

    Tempo changes take effect on the next `process()` call, i.e. at a block
    boundary. Within a block the position is computed from the processed frame
    count, so there is no per-sample drift or accumulated wall-clock error.

    Threading:

    - `process()` is the audio-thread call. It performs a handful of relaxed
      atomic loads/stores and arithmetic: no allocation, locks, logging or
      file I/O.
    - `prepare()`, `reset()`, `rewind()`, the setters, `tapAt()`/`tap()` and
      `applyExternalSync()` are control-thread calls. They publish atomics and
      may be called while the stream runs; a rewind or tempo change is applied
      at the next read.
    - Getters and `snapshot()` are lock-free and safe from any thread. A
      snapshot taken while the audio thread is advancing can mix two adjacent
      blocks, which is fine for meters and UI.

    External sync hook: set the source to `SyncSource::external`, then push
    the measured clock (tempo, running state and position) with
    `applyExternalSync()`. The transport still advances its own position
    between updates, using the external tempo. Feed it once per block from the
    MIDI-clock reader, or whenever the external source reports a change.
*/
class Transport final
{
public:
    /** Where the tempo and playing state come from. */
    enum class SyncSource
    {
        internal,
        external
    };

    /** A consistent-enough view of the transport for UI and API consumers. */
    struct Snapshot
    {
        bool playing = false;
        double bpm = 120.0;
        double ppq = 0.0;
        std::int64_t positionSamples = 0;

        double sampleRate = 0.0;
        int maximumBlockSize = 0;

        SyncSource syncSource = SyncSource::internal;
        bool externalClockLocked = false;

        /** Last tap-tempo estimate in BPM, or 0 before enough taps. */
        double tapTempoBpm = 0.0;
    };

    /** One update from an external clock source (MIDI clock, host, ...). */
    struct ExternalSync
    {
        /** False means the clock was lost: hold position and stop. */
        bool locked = false;

        bool playing = false;
        double bpm = 120.0;
        double ppq = 0.0;
        std::int64_t positionSamples = 0;
    };

    static constexpr double minBpm = 20.0;
    static constexpr double maxBpm = 300.0;
    static constexpr double defaultBpm = 120.0;

    /** Internal positional resolution. Positions are not quantised to it. */
    static constexpr int ppqTicksPerQuarterNote = 960;

    /** How many tap intervals the tap-tempo median keeps. */
    static constexpr int tapHistorySize = 8;

    Transport() = default;
    ~Transport() = default;

    Transport (const Transport&) = delete;
    Transport& operator= (const Transport&) = delete;

    //==============================================================================
    // Setup (control thread)

    /** Stores the device format used to convert frames to musical time.

        The position is not moved. Call `reset()` for a fresh session. Invalid
        values disable position advancement until a valid format is supplied.
    */
    void prepare (double sampleRate, int maximumBlockSize) noexcept;

    /** Returns the musical state to its defaults: 120 BPM, stopped, position
        zero, internal sync, empty tap history. The device format supplied to
        `prepare()` is kept.
    */
    void reset() noexcept;

    /** Returns the musical and sample position to zero without changing tempo,
        playing state or sync source.
    */
    void rewind() noexcept;

    //==============================================================================
    // Transport control (control thread)

    void start() noexcept;
    void stop() noexcept;
    void setPlaying (bool shouldPlay) noexcept;
    bool isPlaying() const noexcept;

    /** Sets the tempo, clamped to `minBpm`..`maxBpm`. Non-finite is ignored. */
    void setBpm (double newBpm) noexcept;
    double getBpm() const noexcept;

    /** Jumps to a musical position, deriving the sample position from the
        current tempo and sample rate.
    */
    void setPpq (double newPpq) noexcept;

    /** Jumps to an absolute sample position, deriving PPQ from the current
        tempo and sample rate.
    */
    void setPositionSamples (std::int64_t newPositionSamples) noexcept;

    double getPpq() const noexcept;
    std::int64_t getPositionSamples() const noexcept;

    /** Device format last supplied to `prepare()`. */
    double getSampleRate() const noexcept;
    int getMaximumBlockSize() const noexcept;

    //==============================================================================
    // Audio thread

    /** Advances the position by `numSamples` frames when playing.

        This is the only method the audio callback must call. It never
        allocates and never blocks.
    */
    void process (int numSamples) noexcept;

    //==============================================================================
    // Musical helpers (any thread)

    /** Frames in one quarter note at the current tempo and sample rate, or 0
        before `prepare()`.
    */
    double getSamplesPerQuarterNote() const noexcept;

    /** Position this many frames into the current block would have.

        Lets a MIDI clock generator schedule tick positions inside a block
        without advancing the transport itself. A non-positive offset returns
        the current PPQ, and a stopped transport does not project forward.
    */
    double getPpqAtSampleOffset (int offsetSamples) const noexcept;

    //==============================================================================
    // Tap tempo (control thread)

    /** Records a tap at a monotonic timestamp in seconds and returns the
        estimate in BPM, or 0 until two valid taps have arrived.

        The estimate is the median of the last `tapHistorySize` intervals, so a
        single late tap does not move the tempo. A gap outside the
        `minBpm`..`maxBpm` range (or a non-increasing timestamp) starts a new
        tap sequence. When the sync source is internal, a successful estimate
        also sets the tempo.
    */
    double tapAt (double timestampSeconds) noexcept;

    /** `tapAt()` using a monotonic wall clock. */
    double tap() noexcept;

    /** Clears the tap history and the last estimate. */
    void resetTapTempo() noexcept;

    /** Last estimate returned by `tapAt()`, or 0. */
    double getTapTempoBpm() const noexcept;

    //==============================================================================
    // External sync hook (control thread)

    void setSyncSource (SyncSource newSource) noexcept;
    SyncSource getSyncSource() const noexcept;

    /** Applies one external clock update.

        Only used when the source is `SyncSource::external`; returns false and
        changes nothing otherwise. A locked update applies the tempo, running
        state and position. An unlocked update marks the clock as lost and
        stops without moving the position or changing the tempo.
    */
    bool applyExternalSync (const ExternalSync& sync) noexcept;

    bool isExternalClockLocked() const noexcept;

    //==============================================================================
    // Inspection (any thread)

    Snapshot snapshot() const noexcept;

private:
    static double clampBpm (double bpm) noexcept;

    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> maximumBlockSize { 0 };

    std::atomic<bool> playing { false };
    std::atomic<double> bpm { defaultBpm };
    std::atomic<double> ppq { 0.0 };
    std::atomic<std::int64_t> positionSamples { 0 };

    std::atomic<SyncSource> syncSource { SyncSource::internal };
    std::atomic<bool> externalClockLocked { false };

    std::atomic<double> tapTempoBpm { 0.0 };

    // Tap state is touched only by the control thread.
    static constexpr double minTapIntervalSeconds = 60.0 / maxBpm;
    static constexpr double maxTapIntervalSeconds = 60.0 / minBpm;

    int tapCount = 0;
    double lastTapTime = 0.0;
    double tapIntervals[tapHistorySize] {};
    int tapIntervalCount = 0;
    int tapIntervalHead = 0;
};

} // namespace AudioEngine
