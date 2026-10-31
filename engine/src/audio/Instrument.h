#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ihub
{

/**
    The instrument seam from `plan/DECISIONS.md`.

    One interface, three implementations: a hosted plugin (`PluginInstrument`),
    the audio input, and the sampler. Everything that makes sound for a slot is
    an `Instrument`, so the graph does not care where the audio comes from.

    Threading:

    - `prepare()` is off the audio thread and may allocate.
    - `noteOn()`, `noteOff()` and `process()` may be called concurrently: note
      events are queued lock-free and consumed by `process()` on the audio
      thread. Implementations must keep `process()` free of allocation, locks
      and logging.
    - `latencySamples()` is safe from any thread.
*/
class Instrument
{
public:
    enum class Kind
    {
        plugin,
        audioInput,
        sampler
    };

    virtual ~Instrument() = default;

    /** Called off the audio thread, before the first `process()` call. */
    virtual void prepare (double sampleRate, int maxBlock) = 0;

    /** Queues a note-on for the next block. `ch` is 1-based (1..16). */
    virtual void noteOn (int ch, int note, float vel) = 0;

    /** Queues a note-off for the next block. `ch` is 1-based (1..16). */
    virtual void noteOff (int ch, int note) = 0;

    /** Renders one block. Audio thread only. */
    virtual void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) = 0;

    /** Processing delay this instrument introduces, in samples. */
    virtual int latencySamples() const = 0;

    virtual Kind kind() const = 0;
};

/** Stable text for logs and tests. */
inline const char* toString (Instrument::Kind kind) noexcept
{
    switch (kind)
    {
        case Instrument::Kind::plugin:     return "plugin";
        case Instrument::Kind::audioInput: return "audio-input";
        case Instrument::Kind::sampler:    return "sampler";
    }

    return "plugin";
}

} // namespace ihub
