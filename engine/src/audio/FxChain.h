#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <atomic>
#include <vector>

namespace AudioEngine
{

/**
    An ordered chain of `juce::AudioProcessor` effects.

    A slot renders its instrument, runs the result through this chain in
    list order, and only then hands it to the bus. Built-in effects and hosted
    VST3 effects are both plain `juce::AudioProcessor` objects, so they share
    exactly one processing path.

    Ownership is deliberately not taken: the caller keeps the processor alive
    for as long as it is in the chain (a `PluginInstrument` for a hosted FX, a
    set-file object for a built-in one). The chain never deletes a processor.

    An empty chain is a bit-exact passthrough: `process()` returns before it
    touches the audio buffer or the MIDI buffer.

    Threading:

    - `prepare()`, `releaseResources()`, `add()`, `insert()`, `remove()`,
      `clear()` and `refreshLatency()` are message-thread calls. Stop the
      stream (or use a block boundary) before changing the chain.
    - `process()` is an audio-thread call: no allocation, no locks, no logging.
    - `latencySamples()` is safe from any thread; it reads a cached value.
*/
class FxChain
{
public:
    FxChain() = default;
    ~FxChain() = default;

    //==============================================================================
    // Setup (off the audio thread)

    /** Prepares every processor in the chain for the engine's format.

        The main input and output buses are matched to `numChannels` when the
        processor supports it, which turns a hosted plugin into an effect. A
        processor that is already prepared can be prepared again after a device
        change.
    */
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);

    /** Releases the processors' audio resources. The stream must be stopped.

        The chain itself keeps its processors and order; a later `prepare()`
        brings them back.
    */
    void releaseResources();

    /** Clears processor state (delay lines, reverb tails, ...).

        Must be called with the stream stopped.
    */
    void reset() noexcept;

    //==============================================================================
    // Chain editing (off the audio thread)

    /** Appends `processor` and returns its index, or -1 for a null pointer.

        A prepared chain prepares the new processor immediately.
    */
    int add (juce::AudioProcessor* processor);

    /** Inserts `processor` at `index` (clamped to the chain length).

        Returns the final index, or -1 for a null pointer.
    */
    int insert (int index, juce::AudioProcessor* processor);

    /** Removes one entry without deleting its processor. */
    bool remove (int index) noexcept;

    /** Removes every entry without deleting the processors. */
    void clear() noexcept;

    int size() const noexcept { return static_cast<int> (processors.size()); }
    bool isEmpty() const noexcept { return processors.empty(); }

    juce::AudioProcessor* getProcessor (int index) const noexcept;
    int indexOf (const juce::AudioProcessor* processor) const noexcept;

    //==============================================================================
    // Latency

    /** Re-reads every processor's reported latency and caches the sum.

        Call off the audio thread after changing a parameter that alters
        latency; `process()` never queries the processors.
    */
    void refreshLatency() noexcept;

    /** Summed latency of the chain, in samples. */
    int latencySamples() const noexcept
    {
        return cachedLatency.load (std::memory_order_relaxed);
    }

    //==============================================================================
    // Audio thread

    /** Runs `buffer` through the chain in order.

        An empty chain returns without modifying `buffer` or `midi`, so an
        empty slot path is bit-exact.
    */
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept;

private:
    void prepareProcessor (juce::AudioProcessor& processor);

    std::vector<juce::AudioProcessor*> processors;

    double sampleRate = 48000.0;
    int maximumBlockSize = 0;
    int numChannels = 0;
    bool prepared = false;

    std::atomic<int> cachedLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChain)
};

} // namespace AudioEngine
