#pragma once

#include "../instruments/Instrument.h"
#include "../audio/Slot.h"

#include "KitLoader.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace ihub::sampler
{

/**
    The sampler as a playable instrument: a fixed polyphonic voice pool over a
    loaded kit.

    The pool is allocated with the instrument and never grows. Every note-on
    claims an idle voice; when all voices are busy the oldest one is stolen and
    counted, so `process()` never allocates, locks or logs. 16 simultaneous
    hits therefore use 16 of the 64 voices and leave the rest available.

    ## Modes

    A pad is either **one-shot** or **gate**:

    - one-shot plays the sample to its end and ignores note-off, which is what
      a drum pad wants;
    - gate keeps the voice alive until note-off and then fades it out over
      `getGateReleaseMilliseconds()` (5 ms by default), so a pad can also be
      played like a short sustained instrument.

    The default is one-shot for every pad.

    ## Choke groups

    Pads may be assigned a choke group (`1..255`; `0` means none). Starting a
    note in a group fades every other sounding voice in the same group over
    `getChokeFadeMilliseconds()` (2 ms by default, capped at 5 ms) before the
    new hit starts. Assigning the open and closed hi-hat to the same group makes
    the closed hat cut the open one, which is the classic drum-machine rule.
    All voices in a group are choked, including retriggers of the same pad.

    ## Threading

    - `setKit()`, the pad settings and `reset()` are control-thread calls; call
      them with the audio stream stopped, exactly like `LoadedKit` itself.
    - `noteOn()` / `noteOff()` push into a fixed lock-free SPSC ring. One
      control/MIDI thread is the producer; `process()` is the consumer and the
      only audio-thread entry point.
    - `process()` obeys the audio-thread contract: no allocation, no locks, no
      logging and no file I/O.
    - The `LoadedKit` passed to `setKit()` must outlive the instrument (or at
      least stay alive until all voices have stopped and the kit is replaced
      with the stream stopped).

    Implements both `ihub::Instrument` and `AudioEngine::SlotSource`, so it can
    be dropped into a slot without an adapter.
*/
class SamplerInstrument final : public Instrument,
                                public AudioEngine::SlotSource
{
public:
    /** Voices in the fixed pool. Must stay comfortably above 16 so ordinary
        drum rolls and layered kits never steal. */
    static constexpr int maxVoices = 64;

    /** Capacity of the lock-free note queue. Overflow drops the event and
        increments `getDroppedEventCount()`; it never blocks. */
    static constexpr int maxPendingEvents = 512;

    enum class PadMode
    {
        oneShot,
        gate
    };

    /** Value that means "not in a choke group". */
    static constexpr int noChokeGroup = 0;

    static constexpr int maximumChokeGroup = 255;

    static constexpr float defaultChokeFadeMilliseconds = 2.0f;
    static constexpr float maximumChokeFadeMilliseconds = 5.0f;
    static constexpr float defaultGateReleaseMilliseconds = 5.0f;

    SamplerInstrument();
    ~SamplerInstrument() override;

    //==============================================================================
    // Setup (off the audio thread)

    /** Points the instrument at a loaded kit. Passing nullptr silences it.

        The instrument does not own the kit. The stream must be stopped while
        this is called; any sounding voices are stopped immediately.
    */
    void setKit (LoadedKit* kit) noexcept;

    const LoadedKit* getKit() const noexcept;

    /** Chooses one-shot or gate for a pad note. Stream stopped. */
    void setPadMode (int note, PadMode mode) noexcept;
    PadMode getPadMode (int note) const noexcept;

    /** Assigns a pad note to a choke group (`0` disables choking). Stream
        stopped. */
    void setChokeGroup (int note, int group) noexcept;
    int getChokeGroup (int note) const noexcept;

    /** Fade applied to voices choked by a new hit in the same group.

        Clamped to 0.1 .. 5 ms; the default is 2 ms, comfortably inside the
        5 ms acceptance window. Off the audio thread.
    */
    void setChokeFadeMilliseconds (float milliseconds) noexcept;
    float getChokeFadeMilliseconds() const noexcept;

    /** Fade applied when a gate-mode voice receives note-off.

        Clamped to 0.1 .. 100 ms; the default is 5 ms. Off the audio thread.
    */
    void setGateReleaseMilliseconds (float milliseconds) noexcept;
    float getGateReleaseMilliseconds() const noexcept;

    /** Stops every voice and discards queued events. Stream stopped. */
    void reset() noexcept;

    //==============================================================================
    // Instrument / AudioEngine::SlotSource

    void prepare (double sampleRate, int maxBlock) override;

    /** Queues a note-on. `vel` is 0..1 (0 is treated as note-off). */
    void noteOn (int ch, int note, float vel) override;

    /** Queues a note-off. Only gate-mode voices react to it. */
    void noteOff (int ch, int note) override;

    /** Queues an all-notes-off controller event for `ch`. */
    void allNotesOff (int ch = 1);

    /** Renders the voice pool into `buffer` and consumes the block's MIDI. */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    /** The sampler introduces no delay. */
    int latencySamples() const noexcept override { return 0; }

    Kind kind() const noexcept override { return Kind::Sampler; }

    //==============================================================================
    // Inspection (control thread)

    int getVoiceCapacity() const noexcept { return maxVoices; }
    int getActiveVoiceCount() const noexcept;

    /** Note-ons that found every voice busy and replaced the oldest. */
    int getStolenVoiceCount() const noexcept;

    /** Queued events that could not be written because the ring was full. */
    int getDroppedEventCount() const noexcept;

    /** Note-ons that actually started a voice. */
    int getStartedVoiceCount() const noexcept;

    /** Events waiting for the next `process()`. */
    int getPendingEventCount() const noexcept;

private:
    struct PendingEvent
    {
        std::uint8_t bytes[3] { 0, 0, 0 };
        std::uint8_t numBytes = 0;
    };

    struct Voice
    {
        const SampleData* sample = nullptr;

        int note = 0;
        int channel = 0;
        int chokeGroup = noChokeGroup;

        double position = 0.0;
        double ratio = 1.0;

        float leftGain = 0.0f;
        float rightGain = 0.0f;

        /** Current linear envelope, 1 while sounding normally. */
        float gain = 1.0f;

        /** Envelope decrease per output sample; 0 while not fading. */
        float fadeStep = 0.0f;

        std::uint64_t serial = 0;

        PadMode mode = PadMode::oneShot;
        bool active = false;
    };

    static int clampNote (int note) noexcept;
    static int clampGroup (int group) noexcept;

    bool pushEvent (const std::uint8_t* bytes, int numBytes) noexcept;
    void drainQueuedEvents() noexcept;
    void applyQueuedEvent (const PendingEvent& event) noexcept;
    void applyMidiEvent (const juce::MidiMessageMetadata& metadata) noexcept;

    void startVoice (int channel, int note, int velocity) noexcept;
    void releaseVoices (int channel, int note) noexcept;
    void chokeVoices (int group, int excludeVoice) noexcept;
    void stopAllVoicesNow() noexcept;
    void allNotesOffNow() noexcept;

    int findFreeVoice() const noexcept;
    int findVoiceToSteal() const noexcept;
    float fadeStepForMilliseconds (float milliseconds) const noexcept;

    void renderVoices (juce::AudioBuffer<float>& buffer,
                       int startSample, int numSamples) noexcept;

    void setVoiceInactive (Voice& voice) noexcept;

    std::atomic<LoadedKit*> kit { nullptr };

    std::array<std::atomic<int>, 128> padModes {};
    std::array<std::atomic<int>, 128> chokeGroups {};

    std::atomic<float> chokeFadeMilliseconds { defaultChokeFadeMilliseconds };
    std::atomic<float> gateReleaseMilliseconds { defaultGateReleaseMilliseconds };

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;

    std::array<Voice, maxVoices> voices {};
    std::uint64_t nextSerial = 1;

    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> stolenVoiceCount { 0 };
    std::atomic<int> droppedEventCount { 0 };
    std::atomic<int> startedVoiceCount { 0 };

    // Lock-free SPSC ring: control thread writes, audio thread reads.
    std::array<PendingEvent, maxPendingEvents> pendingEvents {};
    std::atomic<std::uint32_t> pendingWriteIndex { 0 };
    std::atomic<std::uint32_t> pendingReadIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerInstrument)
};

} // namespace ihub::sampler
