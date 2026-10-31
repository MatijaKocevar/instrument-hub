// ihub-sampler-instrument-test: acceptance check for T092 (pad triggering and
// choke).
//
// The acceptance sentence is:
//
//     16 simultaneous hits do not clip the pool; a closed hat chokes the open
//     hat within 5 ms.
//
// The test generates its own samples, so it needs no committed binaries:
//
// 1. 16 note-ons all start voices in the 64-voice pool, with no steals and no
//    dropped events.
// 2. An open-hat hit is audible, then a closed hat in the same choke group
//    silences it before the 5 ms window closes. The closed-hat sample is
//    digital silence, so any remaining output after the window is the open
//    hat's and the measurement is unambiguous.
// 3. One-shot voices ignore note-off; gate voices release on it.
// 4. Events in the block's MIDI buffer are applied at their sample offsets.
// 5. A full pool steals the oldest voice and never grows.
// 6. process() allocates nothing, so it is safe on the audio thread.

#include "sampler/KitLoader.h"
#include "sampler/SamplerInstrument.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

//==============================================================================
// Counting allocator (same pattern as the other sampler tests).

namespace allocCount
{
std::atomic<bool> enabled { false };
std::atomic<long> total { 0 };

inline void note() noexcept
{
    if (enabled.load (std::memory_order_relaxed))
        total.fetch_add (1, std::memory_order_relaxed);
}
} // namespace allocCount

void* operator new (std::size_t size)
{
    allocCount::note();

    if (void* pointer = std::malloc (size))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* pointer) noexcept { std::free (pointer); }
void operator delete[] (void* pointer) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t) noexcept { std::free (pointer); }

void* operator new (std::size_t size, std::align_val_t alignment)
{
    allocCount::note();

    const auto align = static_cast<std::size_t> (alignment);
    const std::size_t rounded = size == 0 ? align : ((size + align - 1) / align) * align;

    if (void* pointer = std::aligned_alloc (align, rounded))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void operator delete (void* pointer, std::align_val_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::align_val_t) noexcept { std::free (pointer); }
void operator delete (void* pointer, std::size_t, std::align_val_t) noexcept { std::free (pointer); }
void operator delete[] (void* pointer, std::size_t, std::align_val_t) noexcept { std::free (pointer); }

#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
extern "C"
{
    void* __real_malloc (std::size_t);
    void* __wrap_malloc (std::size_t size)
    {
        allocCount::note();
        return __real_malloc (size);
    }

    void* __real_calloc (std::size_t count, std::size_t size);
    void* __wrap_calloc (std::size_t count, std::size_t size)
    {
        allocCount::note();
        return __real_calloc (count, size);
    }

    void* __real_realloc (void* pointer, std::size_t size);
    void* __wrap_realloc (void* pointer, std::size_t size)
    {
        allocCount::note();
        return __real_realloc (pointer, size);
    }
}
#endif

//==============================================================================
namespace
{

using ihub::sampler::KitLoader;
using ihub::sampler::LoadedKit;
using ihub::sampler::SamplerInstrument;

constexpr double engineRate = 48000.0;
constexpr int maximumBlock = 512;
constexpr int fixtureSeconds = 1;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string number (double value, int decimals = 4)
{
    std::cout.setf (std::ios::fixed);
    std::cout.precision (decimals);
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

juce::File testDirectory()
{
    auto directory = juce::File::getCurrentWorkingDirectory().getChildFile ("sampler-instrument-fixtures");
    directory.createDirectory();
    return directory;
}

juce::AudioBuffer<float> makeSine (int frames, double sampleRate, double frequency,
                                   double amplitude = 0.5)
{
    juce::AudioBuffer<float> buffer (1, frames);
    auto* data = buffer.getWritePointer (0);

    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < frames; ++i)
        data[i] = (float) (amplitude * std::sin (twoPi * frequency * (double) i / sampleRate));

    return buffer;
}

juce::AudioBuffer<float> makeSilence (int frames)
{
    juce::AudioBuffer<float> buffer (1, frames);
    buffer.clear();
    return buffer;
}

void saveFloatWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate)
{
    file.deleteFile();

    juce::WavAudioFormat format;
    auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream().release());

    if (stream == nullptr)
    {
        check (false, "fixture write: cannot create " + file.getFullPathName().toStdString());
        return;
    }

    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (rate)
                             .withNumChannels (buffer.getNumChannels())
                             .withBitsPerSample (32)
                             .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    auto writer = format.createWriterFor (stream, options);

    if (writer == nullptr)
    {
        check (false, "fixture write: no writer for " + file.getFileName().toStdString());
        return;
    }

    const bool ok = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    writer.reset();

    check (ok, "fixture write: " + file.getFileName().toStdString());
}

float peakLevel (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax (peak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

    return peak;
}

float peakLevel (const juce::AudioBuffer<float>& buffer, int start, int count)
{
    float peak = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax (peak, buffer.getMagnitude (channel, start, count));

    return peak;
}

std::string fixtureJson()
{
    return R"({
      "schemaVersion": 1,
      "name": "Pad Test Kit",
      "pads": [
        { "note": 36, "name": "Kick",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "kick.wav" } ] } ] },
        { "note": 42, "name": "Closed Hat",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "closed-hat.wav" } ] } ] },
        { "note": 46, "name": "Open Hat",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "open-hat.wav" } ] } ] },
        { "note": 50, "name": "Gate Pad",
          "layers": [ { "minVelocity": 1, "maxVelocity": 127,
                        "roundRobin": [ { "path": "gate.wav" } ] } ] }
      ]
    })";
}

/** Renders exactly `numSamples` and returns the result. */
juce::AudioBuffer<float> render (SamplerInstrument& instrument, int numSamples)
{
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();
    juce::MidiBuffer midi;
    instrument.process (buffer, midi);
    return buffer;
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto directory = testDirectory();

    std::cout << "ihub-sampler-instrument-test\n"
              << "fixtures: " << directory.getFullPathName() << "\n"
              << std::endl;

    //==========================================================================
    // 1. Fixtures.

    const int fixtureFrames = (int) engineRate * fixtureSeconds;

    saveFloatWav (directory.getChildFile ("kick.wav"),
                  makeSine ((int) engineRate / 2, engineRate, 220.0), engineRate);
    saveFloatWav (directory.getChildFile ("closed-hat.wav"),
                  makeSilence ((int) engineRate / 2), engineRate);
    saveFloatWav (directory.getChildFile ("open-hat.wav"),
                  makeSine (fixtureFrames, engineRate, 440.0, 0.5), engineRate);
    saveFloatWav (directory.getChildFile ("gate.wav"),
                  makeSine (fixtureFrames, engineRate, 330.0, 0.5), engineRate);

    LoadedKit kit;
    juce::String error;
    check (KitLoader::loadFromJson (juce::String (fixtureJson()), directory,
                                    engineRate, kit, error),
           "pad test kit loads (" + error.toStdString() + ")");
    check (kit.getKit().pads.size() == 4, "four pads are loaded");

    SamplerInstrument instrument;
    instrument.setKit (&kit);
    instrument.prepare (engineRate, maximumBlock);

    check (instrument.kind() == ihub::Instrument::Kind::Sampler, "kind() reports the sampler");
    check (instrument.latencySamples() == 0, "latency is zero");
    check (instrument.getVoiceCapacity() == 64 && instrument.getVoiceCapacity() > 16,
           "the voice pool holds 64 voices, comfortably above 16");
    check (instrument.getPadMode (36) == SamplerInstrument::PadMode::oneShot,
           "pads default to one-shot");
    check (instrument.getChokeGroup (36) == SamplerInstrument::noChokeGroup,
           "pads default to no choke group");
    check (std::abs (instrument.getChokeFadeMilliseconds() - 2.0f) < 1.0e-6f,
           "the default choke fade is 2 ms");

    //==========================================================================
    // 2. Acceptance part one: 16 simultaneous hits do not clip the pool.

    {
        instrument.reset();

        for (int i = 0; i < 16; ++i)
            instrument.noteOn (1, 36, 1.0f);

        check (instrument.getPendingEventCount() == 16, "16 note-ons are queued");
        const auto block = render (instrument, 128);

        check (instrument.getActiveVoiceCount() == 16,
               "16 simultaneous hits use 16 voices (got "
                   + std::to_string (instrument.getActiveVoiceCount()) + ")");
        check (instrument.getStartedVoiceCount() == 16, "all 16 hits started");
        check (instrument.getStolenVoiceCount() == 0, "no voice was stolen");
        check (instrument.getDroppedEventCount() == 0, "no event was dropped");
        check (peakLevel (block) > 0.1f,
               "the 16-voice block is audible (peak " + number (peakLevel (block)) + ")");

        // One-shots ignore note-off, so all 16 voices survive it.
        for (int i = 0; i < 16; ++i)
            instrument.noteOff (1, 36);

        const auto afterNoteOff = render (instrument, 128);
        check (instrument.getActiveVoiceCount() == 16,
               "one-shot voices survive note-off (got "
                   + std::to_string (instrument.getActiveVoiceCount()) + ")");
        check (peakLevel (afterNoteOff) > 0.1f, "the one-shot voices keep sounding");
    }

    //==========================================================================
    // 3. Acceptance part two: a closed hat chokes the open hat within 5 ms.

    {
        instrument.reset();
        instrument.setChokeGroup (46, 1); // open hat
        instrument.setChokeGroup (42, 1); // closed hat

        instrument.noteOn (1, 46, 1.0f);
        const auto openOnly = render (instrument, 128);
        check (peakLevel (openOnly) > 0.1f,
               "the open hat is audible before the choke (peak "
                   + number (peakLevel (openOnly)) + ")");
        check (instrument.getActiveVoiceCount() == 1, "one open-hat voice is active");

        // The closed-hat sample is digital silence, so everything measured
        // after this point belongs to the open hat. Render the full 5 ms
        // acceptance window.
        instrument.noteOn (1, 42, 1.0f);
        const int windowSamples = (int) std::lround (0.005 * engineRate);
        const auto window = render (instrument, windowSamples);

        const int fadeSamples = (int) std::ceil (
            (double) instrument.getChokeFadeMilliseconds() * 0.001 * engineRate);
        const int tailStart = juce::jmin (windowSamples - 1, fadeSamples);
        const int tailLength = windowSamples - tailStart;
        const auto tailPeak = peakLevel (window, tailStart, tailLength);

        check (instrument.getActiveVoiceCount() == 1,
               "the open-hat voice is gone after the choke (only the silent "
               "closed-hat voice remains)");
        check (tailPeak <= 1.0e-6f,
               "the open hat is silent by 5 ms (tail peak " + number (tailPeak) + ")");

        const auto afterWindow = render (instrument, 128);
        check (peakLevel (afterWindow) <= 1.0e-6f,
               "the choked voice stays silent after the window");
    }

    //==========================================================================
    // 4. One-shot vs gate.

    {
        instrument.reset();
        instrument.setPadMode (50, SamplerInstrument::PadMode::gate);

        instrument.noteOn (1, 50, 1.0f);
        const auto held = render (instrument, 128);
        check (peakLevel (held) > 0.1f, "the gate pad sounds while held");
        check (instrument.getActiveVoiceCount() == 1, "the gate voice is active");

        instrument.noteOff (1, 50);
        const auto released = render (instrument, (int) std::lround (0.020 * engineRate));
        check (instrument.getActiveVoiceCount() == 0,
               "the gate voice stops after note-off (got "
                   + std::to_string (instrument.getActiveVoiceCount()) + ")");
        check (peakLevel (released, 0, released.getNumSamples() / 2) > 0.0f,
               "the gate release is a fade, not an instant cut");
        check (peakLevel (released, released.getNumSamples() / 2,
                          released.getNumSamples() / 2) <= 1.0e-6f,
               "the gate voice is silent by the end of its release");

        instrument.setPadMode (50, SamplerInstrument::PadMode::oneShot);
        instrument.reset();
        instrument.noteOn (1, 36, 1.0f);
        render (instrument, 64);
        instrument.noteOff (1, 36);
        const auto stillPlaying = render (instrument, 64);
        check (instrument.getActiveVoiceCount() == 1 && peakLevel (stillPlaying) > 0.1f,
               "a one-shot ignores note-off and keeps playing");
    }

    //==========================================================================
    // 5. MIDI events in the block are applied at their sample offsets.

    {
        instrument.reset();
        instrument.setPadMode (50, SamplerInstrument::PadMode::gate);

        juce::AudioBuffer<float> buffer (2, 128);
        buffer.clear();
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100), 32);

        instrument.process (buffer, midi);

        check (peakLevel (buffer, 0, 32) <= 1.0e-9f,
               "the block is silent before the MIDI event");
        check (peakLevel (buffer, 32, 96) > 0.1f,
               "the MIDI note-on starts at its sample offset");
        check (instrument.getActiveVoiceCount() == 1, "the MIDI event starts a voice");
    }

    //==========================================================================
    // 6. The pool is fixed: beyond capacity the oldest voice is stolen.

    {
        instrument.reset();

        const int startedBefore = instrument.getStartedVoiceCount();
        const int extra = 4;

        for (int i = 0; i < SamplerInstrument::maxVoices + extra; ++i)
            instrument.noteOn (1, 36, 1.0f);

        render (instrument, 128);

        check (instrument.getActiveVoiceCount() == SamplerInstrument::maxVoices,
               "the active voice count never exceeds the pool (got "
                   + std::to_string (instrument.getActiveVoiceCount()) + ")");
        check (instrument.getStolenVoiceCount() == extra,
               "excess hits steal the oldest voices (got "
                   + std::to_string (instrument.getStolenVoiceCount()) + ")");
        check (instrument.getStartedVoiceCount() - startedBefore
                   == SamplerInstrument::maxVoices + extra,
               "every hit was started, either in a free or a stolen voice");
    }

    //==========================================================================
    // 7. process() is allocation-free.

    {
        instrument.reset();
        instrument.noteOn (1, 46, 1.0f);
        instrument.noteOn (1, 42, 1.0f);

        juce::AudioBuffer<float> buffer (2, maximumBlock);
        juce::MidiBuffer midi;

        allocCount::total.store (0);
        allocCount::enabled.store (true);

        for (int i = 0; i < 1000; ++i)
        {
            if ((i % 16) == 0)
            {
                instrument.noteOn (1, 36, 0.8f);
                instrument.noteOn (1, 46, 0.9f);
            }

            buffer.clear();
            instrument.process (buffer, midi);
        }

        allocCount::enabled.store (false);

        check (allocCount::total.load() == 0,
               "process() allocates nothing (" + std::to_string (allocCount::total.load())
                   + " allocations)");
    }

    //==========================================================================
    std::cout << std::endl;

    if (failures == 0)
    {
        std::cout << "PASS: pad triggering and choke (T092)" << std::endl;
        return 0;
    }

    std::cout << "FAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
