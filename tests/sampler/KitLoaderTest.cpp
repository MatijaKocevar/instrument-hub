// ihub-kit-loader-test: acceptance check for T091 (kit format and velocity
// layers).
//
// The acceptance sentence is:
//
//     a 2-layer kit plays layer 1 below the split velocity and layer 2 above.
//
// The test generates its own samples, so it needs no committed binaries:
//
// 1. A two-layer kit on pad 36 is loaded from JSON. Velocity 40 resolves to
//    the 220 Hz soft sample and velocity 100 to the 880 Hz hard sample; the
//    rendered one-shots measure 220 Hz and 880 Hz, so the split is audible
//    and not only a metadata lookup.
// 2. The split boundary (63/64) and missing notes/velocities behave.
// 3. Round-robin rotates through variants on consecutive hits.
// 4. Pad and sample gain, pan and tune are combined into the resolved hit;
//    +12 semitones renders an octave higher.
// 5. Identical sample paths are decoded once, and a file load resolves paths
//    relative to the kit.
// 6. Every invalid document shape is rejected with an error, and a failed
//    load leaves an already-loaded kit untouched.
// 7. resolve() allocates nothing, so it is safe on the audio thread.

#include "sampler/KitLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
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

using ihub::sampler::Kit;
using ihub::sampler::KitLoader;
using ihub::sampler::KitPad;
using ihub::sampler::LoadedKit;
using ihub::sampler::ResolvedHit;
using ihub::sampler::SampleLoader;
using ihub::sampler::VelocityLayer;

constexpr double engineRate = 48000.0;
constexpr int fixtureFrames = 4800;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

std::string number (double value, int decimals = 3)
{
    std::cout.setf (std::ios::fixed);
    std::cout.precision (decimals);
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

juce::File testDirectory()
{
    auto directory = juce::File::getCurrentWorkingDirectory().getChildFile ("kit-fixtures");
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

double estimateFrequency (const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          int channel = 0)
{
    const int frames = buffer.getNumSamples();

    if (frames < 8)
        return 0.0;

    const int start = frames / 10;
    const int end = frames - frames / 10;
    const auto* data = buffer.getReadPointer (channel);

    double firstCrossing = -1.0;
    double lastCrossing = -1.0;
    int crossings = 0;

    for (int i = start + 1; i < end; ++i)
    {
        const auto previous = data[i - 1];
        const auto current = data[i];

        if (previous <= 0.0f && current > 0.0f)
        {
            const auto fraction = (double) -previous / (double) (current - previous);
            const auto position = (double) (i - 1) + fraction;

            if (firstCrossing < 0.0)
                firstCrossing = position;

            lastCrossing = position;
            ++crossings;
        }
    }

    if (crossings < 2 || lastCrossing <= firstCrossing)
        return 0.0;

    return (double) (crossings - 1) * sampleRate / (lastCrossing - firstCrossing);
}

/** Renders one resolved hit for real: rate conversion by the tune ratio,
    linear interpolation, gain and constant-power pan. The test needs this
    because T091 stops at selection; voice management is T092. */
juce::AudioBuffer<float> renderHit (const ResolvedHit& hit)
{
    if (! hit.isValid())
        return {};

    const auto& source = *hit.sample;
    const auto ratio = hit.playbackRatio > 0.0 ? hit.playbackRatio : 1.0;
    const int outputFrames =
        (int) std::ceil ((double) source.audio.getNumSamples() / ratio);

    juce::AudioBuffer<float> output (2, juce::jmax (1, outputFrames));
    output.clear();

    const auto leftGain = hit.leftGain();
    const auto rightGain = hit.rightGain();
    const int sourceFrames = source.audio.getNumSamples();
    const int sourceChannels = source.audio.getNumChannels();

    for (int n = 0; n < output.getNumSamples(); ++n)
    {
        const double position = (double) n * ratio;
        const auto index = (int) std::floor (position);

        if (index < 0 || index >= sourceFrames)
            break;

        const auto fraction = (float) (position - (double) index);
        const auto nextIndex = juce::jmin (index + 1, sourceFrames - 1);

        const auto interpolate = [&] (int channel)
        {
            const auto* data = source.audio.getReadPointer (juce::jmin (channel, sourceChannels - 1));
            return data[index] + fraction * (data[nextIndex] - data[index]);
        };

        const auto left = interpolate (0);
        const auto right = sourceChannels > 1 ? interpolate (1) : left;

        output.setSample (0, n, left * leftGain);
        output.setSample (1, n, right * rightGain);
    }

    return output;
}

std::string layerLabel (const VelocityLayer* layer)
{
    if (layer == nullptr)
        return "null";

    return std::to_string (layer->minVelocity) + ".." + std::to_string (layer->maxVelocity);
}

bool parseFails (const std::string& json, const std::string& expectedFragment,
                 const std::string& label)
{
    Kit kit;
    juce::String error;
    const bool parsed = KitLoader::parse (juce::String (json), kit, error);

    const bool carriedFragment = error.contains (juce::String (expectedFragment));
    check (! parsed && error.isNotEmpty() && carriedFragment,
           label + " (error: " + error.toStdString() + ")");

    return ! parsed && carriedFragment;
}

std::string baseKitWithLayers (const std::string& firstLayer,
                               const std::string& secondLayer)
{
    return R"({
      "schemaVersion": 1,
      "name": "Test Kit",
      "pads": [
        {
          "note": 36,
          "name": "Kick",
          "layers": [)"
           + firstLayer + "," + secondLayer
           + R"(]
        }
      ]
    })";
}

const std::string softLayer = R"({
  "minVelocity": 1,
  "maxVelocity": 63,
  "roundRobin": [
    { "path": "kick-soft-a.wav" },
    { "path": "kick-soft-b.wav" }
  ]
})";

const std::string hardLayer = R"({
  "minVelocity": 64,
  "maxVelocity": 127,
  "roundRobin": [
    { "path": "kick-hard.wav" }
  ]
})";

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto directory = testDirectory();

    std::cout << "ihub-kit-loader-test\n"
              << "fixtures: " << directory.getFullPathName() << "\n"
              << std::endl;

    //==========================================================================
    // 1. Fixtures: two soft variants and one hard sample.

    saveFloatWav (directory.getChildFile ("kick-soft-a.wav"),
                  makeSine (fixtureFrames, engineRate, 220.0), engineRate);
    saveFloatWav (directory.getChildFile ("kick-soft-b.wav"),
                  makeSine (fixtureFrames, engineRate, 330.0), engineRate);
    saveFloatWav (directory.getChildFile ("kick-hard.wav"),
                  makeSine (fixtureFrames, engineRate, 880.0), engineRate);

    //==========================================================================
    // 2. Schema and parsing.

    {
        std::ifstream schemaFile (IHUB_KIT_SCHEMA);
        const std::string schema ((std::istreambuf_iterator<char> (schemaFile)),
                                  std::istreambuf_iterator<char>());

        check (! schema.empty()
                   && schema.find ("\"schemaVersion\"") != std::string::npos
                   && schema.find ("\"roundRobin\"") != std::string::npos
                   && schema.find ("\"minVelocity\"") != std::string::npos,
               "shared/schema/kit.schema.json describes schemaVersion, layers and roundRobin");
    }

    const auto twoLayerJson = baseKitWithLayers (softLayer, hardLayer);

    Kit parsedKit;
    juce::String error;
    check (KitLoader::parse (juce::String (twoLayerJson), parsedKit, error),
           "two-layer kit parses (" + error.toStdString() + ")");
    check (parsedKit.name == "Test Kit", "kit name is read");
    check (parsedKit.pads.size() == 1, "one pad is read");
    check (parsedKit.findPad (36) != nullptr, "findPad(36) finds the pad");
    check (parsedKit.findPad (37) == nullptr, "findPad(37) returns nullptr");
    check (parsedKit.pads[0].layers.size() == 2, "two layers are read");
    check (parsedKit.pads[0].layerForVelocity (1) != nullptr
               && parsedKit.pads[0].layerForVelocity (63) != nullptr
               && parsedKit.pads[0].layerForVelocity (64) != nullptr
               && parsedKit.pads[0].layerForVelocity (127) != nullptr,
           "velocity lookup covers both layers");

    //==========================================================================
    // 3. Acceptance: the 2-layer kit plays layer 1 below the split and
    //    layer 2 above it.

    LoadedKit kit;
    check (KitLoader::loadFromJson (juce::String (twoLayerJson), directory,
                                    engineRate, kit, error),
           "two-layer kit loads with its samples (" + error.toStdString() + ")");
    check (kit.isValid(), "loaded kit is valid");
    check (kit.getNumLoadedSamples() == 3,
           "three unique samples decoded (got "
               + std::to_string (kit.getNumLoadedSamples()) + ")");

    {
        const auto low = kit.resolve (36, 40);
        check (low.isValid(), "velocity 40 resolves to a sample");
        check (low.layer != nullptr && low.layer->minVelocity == 1
                   && low.layer->maxVelocity == 63,
               "velocity 40 selects layer " + layerLabel (low.layer));
        check (low.sample != nullptr && low.sample->path.contains ("kick-soft-a.wav"),
               "velocity 40 selects the soft sample");
        check (low.pad != nullptr && low.pad->note == 36 && low.note == 36
                   && low.velocity == 40,
               "resolved hit carries pad, note and velocity");
        check (std::abs (low.gain - 1.0f) < 1.0e-6f, "default layer gain is 1");
        check (std::abs (low.pan) < 1.0e-6f, "default pan is centre");
        check (std::abs (low.playbackRatio - 1.0) < 1.0e-9, "default tune leaves pitch alone");

        const auto renderedLow = renderHit (low);
        const auto lowPitch = estimateFrequency (renderedLow, engineRate);
        check (std::abs (lowPitch - 220.0) <= 3.0,
               "layer 1 plays at 220 Hz (measured " + number (lowPitch, 1) + " Hz)");

        const auto high = kit.resolve (36, 100);
        check (high.isValid(), "velocity 100 resolves to a sample");
        check (high.layer != nullptr && high.layer->minVelocity == 64
                   && high.layer->maxVelocity == 127,
               "velocity 100 selects layer " + layerLabel (high.layer));
        check (high.sample != nullptr && high.sample->path.contains ("kick-hard.wav"),
               "velocity 100 selects the hard sample");

        const auto renderedHigh = renderHit (high);
        const auto highPitch = estimateFrequency (renderedHigh, engineRate);
        check (std::abs (highPitch - 880.0) <= 5.0,
               "layer 2 plays at 880 Hz (measured " + number (highPitch, 1) + " Hz)");

        const auto boundaryLow = kit.resolveWithVariant (36, 63, 0);
        const auto boundaryHigh = kit.resolveWithVariant (36, 64, 0);

        check (boundaryLow.layer != nullptr && boundaryLow.layer->minVelocity == 1,
               "velocity 63 is still layer 1");
        check (boundaryHigh.layer != nullptr && boundaryHigh.layer->minVelocity == 64,
               "velocity 64 is already layer 2");
        check (! kit.resolve (37, 100).isValid(), "an unassigned note resolves to nothing");
        check (! kit.resolve (36, 0).isValid(), "velocity 0 resolves to nothing");
    }

    //==========================================================================
    // 4. Round-robin: consecutive hits rotate, and variant selection is
    //    explicit when needed.

    {
        const auto first = kit.resolve (36, 40);
        const auto second = kit.resolve (36, 40);
        const auto third = kit.resolve (36, 40);

        check (first.sample != second.sample, "consecutive hits use different variants");
        check (first.sample == third.sample, "round-robin wraps after two variants");
        check (first.variant == (second.variant + 1) % 2,
               "round-robin advances one variant per hit");

        const auto variantB = kit.resolveWithVariant (36, 40, 1);
        check (variantB.variant == 1 && second.sample != nullptr
                   && variantB.sample != nullptr,
               "resolveWithVariant picks the requested variant");
        check (kit.resolveWithVariant (36, 40, 0).sample
                   != kit.resolveWithVariant (36, 40, 1).sample,
               "the two soft variants are different decoded files");
    }

    //==========================================================================
    // 5. Gain, pan and tune are combined from the pad and the sample.

    {
        const auto tunedJson = R"({
          "schemaVersion": 1,
          "name": "Tune Kit",
          "pads": [
            {
              "note": 38,
              "name": "Snare",
              "gain": 0.5,
              "pan": -1.0,
              "tune": 12.0,
              "layers": [
                {
                  "minVelocity": 1,
                  "maxVelocity": 127,
                  "roundRobin": [
                    { "path": "kick-soft-a.wav", "gain": 0.5, "pan": 1.0, "tune": 0.0 }
                  ]
                }
              ]
            }
          ]
        })";

        LoadedKit tuned;
        juce::String tuneError;
        check (KitLoader::loadFromJson (juce::String (tunedJson), directory,
                                        engineRate, tuned, tuneError),
               "gain/pan/tune kit loads (" + tuneError.toStdString() + ")");

        const auto hit = tuned.resolve (38, 100);
        check (hit.isValid(), "tuned pad resolves");
        check (std::abs (hit.gain - 0.25f) < 1.0e-6f,
               "pad gain 0.5 x sample gain 0.5 = 0.25 (got " + number (hit.gain, 4) + ")");
        check (std::abs (hit.pan) < 1.0e-6f,
               "pad pan -1 + sample pan +1 = centre (got " + number (hit.pan, 4) + ")");
        check (std::abs (hit.playbackRatio - 2.0) < 1.0e-9,
               "12 semitones double the playback rate");

        const auto rendered = renderHit (hit);
        const auto pitch = estimateFrequency (rendered, engineRate);
        check (std::abs (pitch - 440.0) <= 5.0,
               "tuned sample plays an octave higher (measured " + number (pitch, 1) + " Hz)");
        check (std::abs (rendered.getRMSLevel (0, 0, rendered.getNumSamples())
                         - rendered.getRMSLevel (1, 0, rendered.getNumSamples())) < 1.0e-3f,
               "centre pan gives equal left and right level");

        LoadedKit hardLeft;
        juce::String leftError;
        const auto leftJson = R"({
          "schemaVersion": 1,
          "name": "Left Kit",
          "pads": [
            {
              "note": 39,
              "pan": -1.0,
              "layers": [
                { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] }
              ]
            }
          ]
        })";
        check (KitLoader::loadFromJson (juce::String (leftJson), directory,
                                        engineRate, hardLeft, leftError),
               "hard-left kit loads");
        const auto leftHit = hardLeft.resolve (39, 100);
        check (leftHit.leftGain() > 0.9f && std::abs (leftHit.rightGain()) < 1.0e-6f,
               "hard-left pan puts the hit only on the left");
    }

    //==========================================================================
    // 6. File loading, relative paths and sample deduplication.

    {
        const auto file = directory.getChildFile ("dedup.ihkit.json");
        const auto json = R"({
          "schemaVersion": 1,
          "name": "Dedup Kit",
          "pads": [
            {
              "note": 40,
              "layers": [
                { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] }
              ]
            },
            {
              "note": 41,
              "layers": [
                { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] }
              ]
            }
          ]
        })";
        file.replaceWithText (juce::String (json));

        LoadedKit fromFile;
        juce::String fileError;
        check (KitLoader::loadFromFile (file, engineRate, fromFile, fileError),
               "kit file loads relative to its own directory ("
                   + fileError.toStdString() + ")");
        check (fromFile.getNumLoadedSamples() == 1,
               "a shared sample path is decoded once (got "
                   + std::to_string (fromFile.getNumLoadedSamples()) + ")");
        check (fromFile.getLoadedSample (0) != nullptr
                   && fromFile.getKit().findPad (40) != nullptr
                   && fromFile.getKit().findPad (41) != nullptr,
               "both pads resolve from the shared sample");

        check (! KitLoader::loadFromFile (directory.getChildFile ("missing.ihkit.json"),
                                          engineRate, fromFile, fileError),
               "a missing kit file fails");
        check (fileError.isNotEmpty(), "missing kit file fills an error");
    }

    //==========================================================================
    // 7. Invalid documents.

    {
        parseFails ("{ not json", "not valid JSON", "malformed JSON is rejected");

        parseFails (R"({ "schemaVersion": 2, "name": "K", "pads": [] })",
                    "unsupported kit schema version", "unknown schema version is rejected");

        parseFails (R"({ "schemaVersion": 1, "name": "K" })",
                    "missing \"pads\"", "missing pads is rejected");

        parseFails (R"({ "schemaVersion": 1, "name": "K", "pads": [], "extra": 1 })",
                    "unknown property", "unknown properties are rejected");

        const auto withBothLayers = [] (const std::string& padExtra,
                                        const std::string& first,
                                        const std::string& second)
        {
            return R"({
              "schemaVersion": 1,
              "name": "K",
              "pads": [
                { "note": 36, )" + padExtra + R"("layers": [)" + first + ","
                   + second + R"(] }
              ]
            })";
        };

        const std::string full = R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })";
        const std::string lowOnly = R"({ "minVelocity": 1, "maxVelocity": 50, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })";
        const std::string overlap = R"({ "minVelocity": 50, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })";
        const std::string emptyRoundRobin = R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [] })";
        const std::string traversal = R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "../escape.wav" } ] })";

        parseFails (withBothLayers ("", lowOnly, overlap),
                    "overlap", "overlapping velocity layers are rejected");
        parseFails (withBothLayers ("", lowOnly,
                                    R"({ "minVelocity": 80, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })"),
                    "no layer covers velocity", "a velocity gap is rejected");
        parseFails (withBothLayers ("", full, emptyRoundRobin),
                    "no round-robin samples", "an empty round-robin list is rejected");
        parseFails (withBothLayers ("", traversal, full),
                    "unsafe or empty sample path", "path traversal is rejected");
        parseFails (withBothLayers ("", R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav", "gain": 5 } ] })", full),
                    "sample gain must be between 0 and 4", "sample gain out of range is rejected");
        parseFails (withBothLayers ("", R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav", "pan": 1.5 } ] })", full),
                    "sample pan must be between -1 and 1", "sample pan out of range is rejected");
        parseFails (withBothLayers ("", R"({ "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav", "tune": 49 } ] })", full),
                    "sample tune must be between -48 and 48", "sample tune out of range is rejected");
        parseFails (withBothLayers ("", R"({ "minVelocity": 80, "maxVelocity": 20, "roundRobin": [ { "path": "kick-soft-a.wav" } ] })", full),
                    "minVelocity is greater than maxVelocity", "an inverted layer is rejected");
        parseFails (withBothLayers ("\"gain\": 5, ", full, full),
                    "gain must be between 0 and 4", "pad gain out of range is rejected");

        const auto badNote = R"({
          "schemaVersion": 1,
          "name": "K",
          "pads": [
            { "note": 128, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] } ] }
          ]
        })";
        parseFails (badNote, "note must be between 0 and 127",
                    "an out-of-range pad note is rejected");

        const auto duplicateNote = R"({
          "schemaVersion": 1,
          "name": "K",
          "pads": [
            { "note": 36, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-soft-a.wav" } ] } ] },
            { "note": 36, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "kick-hard.wav" } ] } ] }
          ]
        })";
        parseFails (duplicateNote, "duplicate pad note", "duplicate pad notes are rejected");

        const auto missingSample = R"({
          "schemaVersion": 1,
          "name": "K",
          "pads": [
            { "note": 36, "layers": [ { "minVelocity": 1, "maxVelocity": 127, "roundRobin": [ { "path": "no-such-file.wav" } ] } ] }
          ]
        })";

        LoadedKit missing;
        juce::String missingError;
        check (! KitLoader::loadFromJson (juce::String (missingSample), directory,
                                          engineRate, missing, missingError)
                   && missingError.contains ("no-such-file.wav"),
               "a missing sample fails with its path in the error ("
                   + missingError.toStdString() + ")");
        check (! missing.isValid(), "a failed load leaves an empty kit");

        // A failed load must not disturb an already-loaded kit.
        const auto nameBefore = kit.getKit().name;
        LoadedKit survivor = std::move (kit);
        juce::String survivorError;

        check (! KitLoader::loadFromJson (juce::String (missingSample), directory,
                                          engineRate, survivor, survivorError),
               "loading a broken kit into a loaded one fails");
        check (survivor.getKit().name == nameBefore && survivor.isValid(),
               "a failed load leaves the existing kit untouched");

        juce::String rateError;
        check (! KitLoader::loadFromJson (juce::String (twoLayerJson), directory,
                                          0.0, survivor, rateError),
               "an invalid engine sample rate is rejected");
        check (survivor.getKit().name == nameBefore,
               "invalid sample rate also leaves the existing kit untouched");

        kit = std::move (survivor);
    }

    //==========================================================================
    // 8. resolve() is allocation-free.

    {
        allocCount::total.store (0);
        allocCount::enabled.store (true);

        volatile float sink = 0.0f;

        for (int i = 0; i < 20000; ++i)
        {
            const auto hit = kit.resolve (36, (i % 127) + 1);

            if (hit.isValid())
                sink += hit.gain + hit.pan + (float) hit.playbackRatio;
        }

        allocCount::enabled.store (false);

        check (allocCount::total.load() == 0,
               "resolve() allocates nothing (" + std::to_string (allocCount::total.load())
                   + " allocations)");
        (void) sink;
    }

    //==========================================================================
    std::cout << std::endl;

    if (failures == 0)
    {
        std::cout << "PASS: kit format and velocity layers (T091)" << std::endl;
        return 0;
    }

    std::cout << "FAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
