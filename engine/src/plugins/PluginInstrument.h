#pragma once

#include "../audio/Instrument.h"

#include "../audio/Slot.h"

// The headless module is enough for hosting (no editor yet), and keeps the
// wrapper linkable from console tests as well as from the GUI application.
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace ihub::plugins
{

/**
    A hosted VST3 (or any format the caller's manager knows) as an Instrument.

    The wrapper owns one `juce::AudioPluginInstance` and adds everything the
    engine needs:

    - **Load / unload.** `load()` instantiates from a `PluginDescription`,
      picks a bus layout that matches the engine's channel count, and prepares
      the instance for the requested sample rate and block size. `unload()`
      destroys it. Both are message-thread calls and must not run while the
      audio stream is calling `process()`.
    - **MIDI in.** `noteOn()` / `noteOff()` (and the generic `queueMidi()`)
      push into a fixed-capacity lock-free SPSC ring. `process()` drains the
      ring into a pre-allocated `MidiBuffer` on the audio thread, merging it
      with the block's incoming MIDI. Nothing allocates, locks or logs there.
    - **State.** `getStateInformation()` / `setStateInformation()` forward to
      the plugin and give the caller a plain `juce::MemoryBlock`, which is what
      the set file stores as `plugin-state/<uuid>.bin`.
    - **Parameters.** A small normalised-value facade so the UI and tests never
      touch raw `AudioProcessorParameter` objects.

    It implements `ihub::Instrument` (the DECISIONS interface) *and*
    `AudioEngine::SlotSource`, so a slot can process it directly today and
    T030's adapter is not required for plugin instruments.
*/
class PluginInstrument final : public Instrument,
                               public AudioEngine::SlotSource
{
public:
    /** Capacity of the lock-free note queue. Overflow drops the event and
        increments `getDroppedMidiEventCount()`; it never blocks. */
    static constexpr int maxPendingMidiEvents = 256;

    /** Pre-allocated MIDI scratch space used when queued notes have to be
        merged with the block's incoming MIDI. */
    static constexpr size_t midiScratchBytes = 16384;

    PluginInstrument();
    ~PluginInstrument() override;

    //==============================================================================
    // Loading (message thread)

    /** Instantiates `description` and leaves the instrument ready for
        `prepare()`. On failure returns false and fills `error`. */
    bool load (juce::AudioPluginFormatManager& formatManager,
               const juce::PluginDescription& description,
               double initialSampleRate,
               int initialBufferSize,
               juce::String& error);

    /** Instantiates the first plugin found in a bundle (`.vst3` directory or
        file). */
    bool loadFromFile (juce::AudioPluginFormatManager& formatManager,
                       const juce::File& bundle,
                       double initialSampleRate,
                       int initialBufferSize,
                       juce::String& error);

    /** Convenience: `load()` into a freshly created wrapper. */
    static std::unique_ptr<PluginInstrument> create (juce::AudioPluginFormatManager& formatManager,
                                                     const juce::PluginDescription& description,
                                                     double initialSampleRate,
                                                     int initialBufferSize,
                                                     juce::String& error);

    /** Destroys the plugin instance and clears the note queue.

        The audio stream must be stopped: the slot graph must not call
        `process()` while this runs.
    */
    void unload();

    /** Installs a callback that `unload()` (and therefore `load()` when it
        replaces an instance) invokes on the message thread just before the
        hosted processor is destroyed.

        This is the seam `PluginWindow` uses: JUCE asserts when an
        `AudioProcessor` is deleted while it still has an active editor, so the
        window closes itself here. The callback is moved out of the wrapper
        before it runs, so it may safely re-enter `setBeforeUnloadCallback()`.
    */
    void setBeforeUnloadCallback (std::function<void()> callback);

    bool isLoaded() const noexcept { return plugin != nullptr; }

    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    /** Description of the loaded type; only valid while loaded. */
    const juce::PluginDescription& getPluginDescription() const noexcept { return description; }

    juce::String getName() const;

    //==============================================================================
    // Bus layout and preparation (message thread)

    /** Selects a main bus layout that matches `numChannels` (1 = mono,
        2 = stereo) and returns false when the plugin cannot provide it.

        Called automatically by `load()` with the engine's default of two
        channels; a mono engine can call it again before `prepare()`.
    */
    bool configureBuses (int numChannels = 2);

    /** Engine channel count requested from `configureBuses()`. */
    int getNumChannels() const noexcept { return numChannels; }

    /** Forwards rate/block size to the plugin, prepares it, and caches the
        latency the plugin reports. Safe to call again after a device change or
        after loading a state blob.
    */
    void prepare (double sampleRate, int maxBlock) override;

    /** Re-reads the plugin's latency. Call off the audio thread after changing
        a parameter that alters it. */
    void refreshLatency();

    /** Latency cached by the last `prepare()` / `refreshLatency()`. Safe from
        the audio thread; DECISIONS says not to query the plugin per block. */
    int latencySamples() const noexcept override;

    Instrument::Kind kind() const noexcept override { return Instrument::Kind::plugin; }

    //==============================================================================
    // MIDI in

    void noteOn (int ch, int note, float vel) override;
    void noteOff (int ch, int note) override;

    /** Queues an arbitrary short MIDI message (up to three bytes). */
    bool queueMidi (const juce::uint8* bytes, int numBytes);

    /** Queues an all-notes-off controller message. */
    void allNotesOff (int ch = 1);

    /** Events waiting for the next `process()`. Safe from any thread. */
    int getPendingMidiEventCount() const noexcept;

    /** Events handed to the plugin since construction (diagnostics/tests). */
    int getDeliveredMidiEventCount() const noexcept;

    /** Events dropped because the queue was full. */
    int getDroppedMidiEventCount() const noexcept;

    /** Discards queued events. Stream must be stopped. */
    void clearPendingMidi() noexcept;

    //==============================================================================
    // Audio thread

    /** Renders the plugin into `buffer`.

        If nothing is loaded, the buffer is cleared. The plugin's channel count
        is matched against the buffer: a plugin with fewer channels gets a view
        of the first channels, so an effect never reads past the engine buffer.
    */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    //==============================================================================
    // State (message thread)

    /** The plugin's opaque state, ready to be written to a `.bin` chunk. */
    juce::MemoryBlock getStateInformation() const;

    /** Same, writing into an existing block. */
    void getStateInformation (juce::MemoryBlock& destination) const;

    bool setStateInformation (const void* data, int numBytes);
    bool setStateInformation (const juce::MemoryBlock& block);

    //==============================================================================
    // Parameters (message thread)

    int getNumParameters() const;

    /** Finds a parameter by `HostedAudioProcessorParameter::getParameterID()`
        or by its display name (case-insensitive). */
    juce::AudioProcessorParameter* findParameter (const juce::String& idOrName) const;

    /** Sets a normalised (0..1) value, notifying the host. */
    bool setParameterValue (const juce::String& idOrName, float normalisedValue);

    /** Reads a normalised (0..1) value, or -1 when the parameter is missing. */
    float getParameterValue (const juce::String& idOrName) const;

    /** Human-readable value, e.g. "10.0 ms". Empty when missing. */
    juce::String getParameterText (const juce::String& idOrName) const;

private:
    struct PendingMidiEvent
    {
        std::uint8_t bytes[3] { 0, 0, 0 };
        std::uint8_t numBytes = 0;
    };

    bool pushEvent (const juce::uint8* bytes, int numBytes) noexcept;
    void drainPendingMidi (juce::MidiBuffer& destination) noexcept;

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    int numChannels = 2;

    std::atomic<int> cachedLatency { 0 };

    // Lock-free SPSC ring for note events. The producer is the control thread,
    // the consumer is the audio thread.
    std::array<PendingMidiEvent, maxPendingMidiEvents> pendingMidi {};
    std::atomic<std::uint32_t> pendingWriteIndex { 0 };
    std::atomic<std::uint32_t> pendingReadIndex { 0 };
    std::atomic<int> deliveredMidiEvents { 0 };
    std::atomic<int> droppedMidiEvents { 0 };

    juce::MidiBuffer midiScratch;

    std::function<void()> beforeUnloadCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginInstrument)
};

} // namespace ihub::plugins
