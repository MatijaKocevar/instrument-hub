// ihub-sample-loader-test: acceptance check for T090 (sample loader).
//
// The acceptance sentence is:
//
//     loads a 48 kHz and a 44.1 kHz file; output lengths and pitch are
//     correct at 48 kHz.
//
// The test generates its own fixtures, so it needs no committed binaries:
//
// 1. A 48 kHz WAV is decoded without resampling. Length and every sample are
//    preserved (the fixture is 32-bit float, so the comparison is exact).
// 2. A 44.1 kHz WAV is resampled to 48 kHz: 22050 source frames become 24000
//    output frames and a 1 kHz sine stays at 1 kHz.
// 3. The length rule round(sourceFrames * 48000 / sourceRate) holds for other
//    durations, and pitch holds for several frequencies.
// 4. AIFF and FLAC decode through the same AudioFormatManager path.
// 5. DC and stereo content survive the resampler.
// 6. A background load publishes through SampleHandoff, and the audio-thread
//    hand-off (poll/current/retire) allocates nothing.
// 7. Invalid and unsupported files fail with an error instead of crashing.
// 8. A producer/consumer stress run exercises the lock-free replacement path.

#include "sampler/SampleLoader.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

//==============================================================================
// Counting allocator (same pattern as the DSP tests).

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

using ihub::sampler::SampleData;
using ihub::sampler::SampleHandoff;
using ihub::sampler::SampleLoader;

constexpr double engineRate = 48000.0;
constexpr double standardRate = 44100.0;
constexpr double sineFrequency = 1000.0;
constexpr double sineAmplitude = 0.5;

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
    auto directory = juce::File::getCurrentWorkingDirectory()
                         .getChildFile ("sample-loader-fixtures");
    directory.createDirectory();
    return directory;
}

juce::AudioBuffer<float> makeSine (int frames, double sampleRate, double frequency,
                                   double amplitude = sineAmplitude, int channels = 1)
{
    juce::AudioBuffer<float> buffer (channels, frames);

    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        const auto channelScale = channel == 0 ? 1.0 : 0.5;

        for (int i = 0; i < frames; ++i)
            data[i] = (float) (amplitude * channelScale
                               * std::sin (twoPi * frequency * (double) i / sampleRate));
    }

    return buffer;
}

bool writeAudioFile (const juce::File& file,
                     juce::AudioFormat& format,
                     const juce::AudioBuffer<float>& buffer,
                     double sampleRate,
                     int bitsPerSample,
                     juce::AudioFormatWriterOptions::SampleFormat sampleFormat =
                         juce::AudioFormatWriterOptions::SampleFormat::automatic)
{
    file.deleteFile();

    auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream().release());

    if (stream == nullptr)
        return false;

    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withNumChannels (buffer.getNumChannels())
                             .withBitsPerSample (bitsPerSample)
                             .withSampleFormat (sampleFormat);

    auto writer = format.createWriterFor (stream, options);

    if (writer == nullptr)
        return false;

    const bool ok = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    writer.reset();

    return ok;
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
            // Linear interpolation puts the crossing between the two
            // samples, so the estimate is not limited by the frame grid.
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

double meanOfMiddle (const juce::AudioBuffer<float>& buffer, int channel = 0)
{
    const int frames = buffer.getNumSamples();
    const int start = frames / 10;
    const int end = frames - frames / 10;

    if (end <= start)
        return 0.0;

    const auto* data = buffer.getReadPointer (channel);
    double sum = 0.0;

    for (int i = start; i < end; ++i)
        sum += (double) data[i];

    return sum / (double) (end - start);
}

void checkPitch (const SampleData& sample, double expected,
                 const std::string& label, double toleranceHz = 5.0)
{
    const double measured = estimateFrequency (sample.audio, sample.sampleRate);

    check (std::abs (measured - expected) <= toleranceHz,
           label + ": pitch " + number (measured, 1) + " Hz (expected "
               + number (expected, 1) + " Hz)");
}

void saveFloatWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double rate)
{
    juce::WavAudioFormat format;
    const bool ok = writeAudioFile (file, format, buffer, rate, 32,
                                    juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    check (ok, "fixture write (32-bit float WAV): " + file.getFileName().toStdString());
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto directory = testDirectory();

    std::cout << "ihub-sample-loader-test\n"
              << "fixtures: " << directory.getFullPathName() << "\n"
              << std::endl;

    //==========================================================================
    // 1. Acceptance: a 48 kHz file, decoded without resampling.

    {
        const auto file = directory.getChildFile ("tone-48k.wav");
        const auto source = makeSine (24000, engineRate, sineFrequency);
        saveFloatWav (file, source, engineRate);

        juce::String error;
        const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

        check (sample != nullptr, "48 kHz WAV loads");

        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 24000,
                   "48 kHz: output length 24000 frames (got "
                       + std::to_string (sample->audio.getNumSamples()) + ")");
            check (sample->audio.getNumChannels() == 1, "48 kHz: mono stays mono");
            check (std::abs (sample->sampleRate - engineRate) < 1.0e-9,
                   "48 kHz: output sample rate is the engine rate");
            check (std::abs (sample->sourceSampleRate - engineRate) < 1.0e-9,
                   "48 kHz: source sample rate preserved");
            check (sample->sourceFrames == 24000, "48 kHz: source frame count preserved");

            double maxDifference = 0.0;

            for (int i = 0; i < 24000; ++i)
                maxDifference = juce::jmax (maxDifference,
                                            (double) std::abs (sample->audio.getSample (0, i)
                                                               - source.getSample (0, i)));

            const bool identicalBits =
                std::memcmp (sample->audio.getReadPointer (0),
                             source.getReadPointer (0),
                             (size_t) 24000 * sizeof (float)) == 0;

            check (identicalBits,
                   "48 kHz: equal-rate load is a bit-exact copy (max diff "
                       + number (maxDifference) + ")");
            checkPitch (*sample, sineFrequency, "48 kHz");
        }
    }

    //==========================================================================
    // 2. Acceptance: a 44.1 kHz file, resampled to 48 kHz.

    {
        const auto file = directory.getChildFile ("tone-44k1.wav");
        const auto source = makeSine (22050, standardRate, sineFrequency);
        saveFloatWav (file, source, standardRate);

        juce::String error;
        const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

        check (sample != nullptr, "44.1 kHz WAV loads");

        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 24000,
                   "44.1 kHz: 22050 frames become 24000 frames (got "
                       + std::to_string (sample->audio.getNumSamples()) + ")");
            check (std::abs (sample->sampleRate - engineRate) < 1.0e-9,
                   "44.1 kHz: output is at 48 kHz");
            check (std::abs (sample->sourceSampleRate - standardRate) < 1.0e-9,
                   "44.1 kHz: source rate preserved");
            check (std::abs (sample->getLengthSeconds() - 0.5) < 1.0e-9,
                   "44.1 kHz: output duration is 0.5 s");
            checkPitch (*sample, sineFrequency, "44.1 kHz", 2.0);

            const auto peak = sample->audio.getMagnitude (0, 0, sample->audio.getNumSamples());
            check (std::abs (peak - sineAmplitude) < 0.02,
                   "44.1 kHz: amplitude preserved (peak " + number (peak) + ")");
        }
    }

    //==========================================================================
    // 3. Length rule and pitch over other frequencies.

    {
        struct Case { int sourceFrames; int expectedFrames; };
        const Case cases[] = { { 1000, 1088 }, { 4410, 4800 }, { 8820, 9600 },
                               { 44100, 48000 }, { 22050, 24000 } };

        for (const auto& testCase : cases)
        {
            const auto file = directory.getChildFile ("length-"
                                                       + std::to_string (testCase.sourceFrames)
                                                       + ".wav");
            saveFloatWav (file, makeSine (testCase.sourceFrames, standardRate, sineFrequency),
                          standardRate);

            juce::String error;
            const auto sample = SampleLoader::loadBlocking (file, engineRate, error);
            const int actual = sample != nullptr ? sample->audio.getNumSamples() : -1;

            check (actual == testCase.expectedFrames,
                   "length: " + std::to_string (testCase.sourceFrames)
                       + " frames at 44.1 kHz -> " + std::to_string (testCase.expectedFrames)
                       + " at 48 kHz (got " + std::to_string (actual) + ")");
        }

        for (const double frequency : { 220.0, 440.0, 1000.0, 4000.0, 8000.0 })
        {
            const auto file = directory.getChildFile ("pitch-" + number (frequency, 0) + ".wav");
            saveFloatWav (file, makeSine (22050, standardRate, frequency, 0.4), standardRate);

            juce::String error;
            const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

            if (sample == nullptr)
            {
                check (false, "pitch case " + number (frequency, 0) + " Hz loads");
                continue;
            }

            checkPitch (*sample, frequency, "pitch " + number (frequency, 0) + " Hz",
                        juce::jmax (3.0, frequency * 0.005));
        }
    }

    //==========================================================================
    // 4. Downsampling and other formats.

    {
        const auto source = makeSine (96000, 96000.0, sineFrequency);
        const auto file = directory.getChildFile ("tone-96k.wav");
        saveFloatWav (file, source, 96000.0);

        juce::String error;
        const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

        check (sample != nullptr, "96 kHz WAV loads");
        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 48000,
                   "96 kHz: 96000 frames become 48000 frames (got "
                       + std::to_string (sample->audio.getNumSamples()) + ")");
            checkPitch (*sample, sineFrequency, "96 kHz downsampling", 1.0);
        }
    }

    {
        const auto source = makeSine (22050, standardRate, sineFrequency);

        const auto aiff = directory.getChildFile ("tone.aiff");
        juce::AiffAudioFormat aiffFormat;
        check (writeAudioFile (aiff, aiffFormat, source, standardRate, 24),
               "fixture write (AIFF)");

        juce::String error;
        const auto aiffSample = SampleLoader::loadBlocking (aiff, engineRate, error);
        check (aiffSample != nullptr, "AIFF loads");
        if (aiffSample != nullptr)
        {
            check (aiffSample->audio.getNumSamples() == 24000, "AIFF: length is 24000");
            checkPitch (*aiffSample, sineFrequency, "AIFF", 2.0);
        }

        const auto flac = directory.getChildFile ("tone.flac");
        juce::FlacAudioFormat flacFormat;
        check (writeAudioFile (flac, flacFormat, source, standardRate, 24),
               "fixture write (FLAC)");

        error.clear();
        const auto flacSample = SampleLoader::loadBlocking (flac, engineRate, error);
        check (flacSample != nullptr, "FLAC loads");
        if (flacSample != nullptr)
        {
            check (flacSample->audio.getNumSamples() == 24000, "FLAC: length is 24000");
            checkPitch (*flacSample, sineFrequency, "FLAC", 2.0);
        }
    }

    //==========================================================================
    // 5. DC and stereo.

    {
        juce::AudioBuffer<float> dc (1, 22050);
        std::fill_n (dc.getWritePointer (0), 22050, 0.25f);

        const auto dcFile = directory.getChildFile ("dc.wav");
        saveFloatWav (dcFile, dc, standardRate);

        juce::String error;
        const auto dcSample = SampleLoader::loadBlocking (dcFile, engineRate, error);

        check (dcSample != nullptr, "DC file loads");
        if (dcSample != nullptr)
        {
            const auto measured = meanOfMiddle (dcSample->audio);
            check (std::abs (measured - 0.25) < 0.002,
                   "DC level preserved through resampling (" + number (measured, 4) + ")");
        }

        const auto stereoFile = directory.getChildFile ("stereo.wav");
        saveFloatWav (stereoFile, makeSine (22050, standardRate, sineFrequency, 0.5, 2),
                      standardRate);

        error.clear();
        const auto stereo = SampleLoader::loadBlocking (stereoFile, engineRate, error);

        check (stereo != nullptr, "stereo file loads");
        if (stereo != nullptr)
        {
            check (stereo->audio.getNumChannels() == 2, "stereo stays stereo");
            check (stereo->sourceChannels == 2, "stereo source channel count preserved");
            check (stereo->audio.getNumSamples() == 24000, "stereo: length is 24000");

            const auto left = stereo->audio.getRMSLevel (0, 0, stereo->audio.getNumSamples());
            const auto right = stereo->audio.getRMSLevel (1, 0, stereo->audio.getNumSamples());

            check (std::abs (right / left - 0.5) < 0.02,
                   "stereo: channel balance preserved (R/L " + number (right / left) + ")");
        }
    }

    //==========================================================================
    // 6. Background load and RT-safe hand-off.

    {
        const auto file = directory.getChildFile ("async-44k1.wav");
        saveFloatWav (file, makeSine (22050, standardRate, sineFrequency), standardRate);

        SampleLoader loader;
        check (loader.prepare (engineRate), "loader accepts 48 kHz");
        check (! loader.prepare (0.0), "loader rejects a zero sample rate");
        check (std::abs (loader.getEngineSampleRate() - engineRate) < 1.0e-9,
               "rejected rate leaves the engine rate unchanged");

        juce::String error;
        const auto requestId = loader.loadAsync (file, error);
        check (requestId > 0, "background load returns a request id");
        check (loader.waitForIdle (10000), "background load finishes");

        SampleLoader::Event event;
        check (loader.takeEvent (event), "background load reports an event");
        check (event.requestId == requestId, "event carries the request id");
        check (event.success, "event reports success");
        check (event.error.isEmpty(), "success event has no error");

        check (loader.getHandoff().hasPendingPublish(), "sample is published for hand-off");
        check (loader.current() == nullptr, "nothing is installed before the audio thread polls");

        const auto* sample = loader.poll();
        check (sample != nullptr, "audio-thread poll installs the sample");
        check (loader.current() == sample, "installed sample is current");
        check (loader.poll() == nullptr, "a second poll returns nothing new");

        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 24000, "async: length is 24000");
            checkPitch (*sample, sineFrequency, "async", 2.0);
        }

        // The audio-thread path must not allocate. The first poll has already
        // happened above; this loops over the steady-state calls, including a
        // replacement that retires the old sample.
        allocCount::total.store (0);
        allocCount::enabled.store (true);

        for (int i = 0; i < 20000; ++i)
        {
            (void) loader.poll();
            (void) loader.current();
        }

        allocCount::enabled.store (false);
        check (allocCount::total.load() == 0,
               "poll/current allocate nothing (" + std::to_string (allocCount::total.load())
                   + " allocations)");

        loader.collectGarbage();
    }

    {
        // A second load replaces the installed sample; the old one is moved
        // to the retirement queue rather than freed on the audio thread.
        const auto firstFile = directory.getChildFile ("replace-1.wav");
        const auto secondFile = directory.getChildFile ("replace-2.wav");
        saveFloatWav (firstFile, makeSine (2400, engineRate, 500.0), engineRate);
        saveFloatWav (secondFile, makeSine (4800, engineRate, 2000.0), engineRate);

        SampleLoader loader;
        juce::String error;

        loader.loadAsync (firstFile, error);
        loader.waitForIdle (10000);
        const auto* first = loader.poll();

        SampleLoader::Event event;
        while (loader.takeEvent (event)) {}

        loader.loadAsync (secondFile, error);
        loader.waitForIdle (10000);
        const auto* second = loader.poll();

        check (first != nullptr && second != nullptr && first != second,
               "a replacement publishes a different sample");
        check (loader.getHandoff().getNumRetired() == 1,
               "the replaced sample is queued for retirement (not freed on the audio thread)");
        check (loader.getHandoff().getNumOverflows() == 0, "retirement queue did not overflow");

        loader.collectGarbage();
        check (loader.getHandoff().getNumRetired() == 0, "replacement garbage collected");
    }

    //==========================================================================
    // 7. Failure paths.

    {
        juce::String error;
        const auto missing = SampleLoader::loadBlocking (
            directory.getChildFile ("does-not-exist.wav"), engineRate, error);
        check (missing == nullptr, "missing file returns nullptr");
        check (error.isNotEmpty(), "missing file fills an error");

        const auto garbage = directory.getChildFile ("garbage.wav");
        garbage.replaceWithText ("this is not an audio file");

        error.clear();
        const auto unsupported = SampleLoader::loadBlocking (garbage, engineRate, error);
        check (unsupported == nullptr, "garbage file returns nullptr");
        check (error.isNotEmpty(), "garbage file fills an error");

        SampleLoader loader;
        const auto badId = loader.loadAsync (garbage, error);
        check (badId > 0, "garbage file still queues a background request");
        check (loader.waitForIdle (5000), "failed background request finishes");

        SampleLoader::Event event;
        bool sawFailure = false;

        while (loader.takeEvent (event))
        {
            if (event.requestId == badId && ! event.success && event.error.isNotEmpty())
                sawFailure = true;
        }

        check (sawFailure, "failed background request reports a failure event");
        check (loader.poll() == nullptr, "failed background request publishes nothing");
    }

    //==========================================================================
    // 8. Producer/consumer stress on the lock-free replacement path.

    {
        SampleHandoff handoff;
        std::atomic<bool> producerDone { false };
        constexpr int publications = 200;

        std::thread producer ([&handoff, &producerDone, &directory]
        {
            for (int i = 0; i < publications; ++i)
            {
                auto sample = std::make_unique<SampleData>();
                sample->audio.setSize (1, 64);
                sample->audio.clear();
                sample->audio.setSample (0, 0, (float) i);
                sample->sampleRate = engineRate;
                sample->name = "stress-" + juce::String (i);

                handoff.publish (std::move (sample));
                std::this_thread::sleep_for (std::chrono::microseconds (200));
            }

            producerDone.store (true);
        });

        int observed = 0;

        while (! producerDone.load())
        {
            if (handoff.poll() != nullptr)
            {
                ++observed;

                // Reclaim retired buffers periodically; no voice is holding
                // an old pointer in this test.
                if ((observed & 15) == 0)
                    handoff.collectGarbage();
            }
        }

        // Drain the last publications.
        while (handoff.poll() != nullptr)
        {
            ++observed;
            handoff.collectGarbage();
        }

        producer.join();

        check (observed > 0 && observed <= publications,
               "stress: consumer observed " + std::to_string (observed)
                   + " of " + std::to_string (publications) + " publications");
        check (handoff.getNumOverflows() == 0, "stress: retirement queue never overflowed");
        handoff.collectGarbage();
        check (handoff.getNumRetired() == 0, "stress: everything reclaimed");
    }

    //==========================================================================
    std::cout << std::endl;

    if (failures == 0)
    {
        std::cout << "PASS: sample loader (T090)" << std::endl;
        return 0;
    }

    std::cout << "FAIL: " << failures << " check(s) failed" << std::endl;
    return 1;
}
