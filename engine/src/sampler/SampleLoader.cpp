#include "SampleLoader.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ihub::sampler
{

namespace
{

//==============================================================================
// Resampler

/** Number of zero crossings on each side of the windowed-sinc kernel at the
    engine Nyquist. 24 gives a transition band narrow enough for sample
    playback while keeping a 44.1 -> 48 kHz conversion at 48 taps per output
    sample. */
constexpr int resamplerZeroCrossings = 24;

/** Phase-table resolution. Two adjacent rows are linearly interpolated, so
    512 rows keep the interpolation error far below the 24-bit decode floor. */
constexpr int resamplerPhases = 512;

/** The windowed-sinc table for one (source rate, engine rate) pair.

    For each fractional phase it stores an already DC-normalised kernel. The
    kernel's cutoff is the lower of the two Nyquist frequencies, expressed in
    cycles per source sample, so downsampling is band-limited instead of
    aliasing.
*/
struct SincTable
{
    SincTable (double sourceRate, double destinationRate)
    {
        const auto nyquistRatio = destinationRate / juce::jmax (1.0, sourceRate);
        cutoff = juce::jmin (0.5, 0.5 * nyquistRatio);

        halfWidth = juce::jmax (1, (int) std::ceil (resamplerZeroCrossings / (2.0 * cutoff)));
        taps = halfWidth * 2;

        weights.resize ((size_t) (resamplerPhases + 1) * (size_t) taps);

        const double twoFc = 2.0 * cutoff;

        for (int phase = 0; phase <= resamplerPhases; ++phase)
        {
            const double fraction = (double) phase / (double) resamplerPhases;
            float* row = weights.data() + (size_t) phase * (size_t) taps;
            double sum = 0.0;

            for (int tap = 0; tap < taps; ++tap)
            {
                // x is the distance in source samples between the output
                // position and this input sample.
                const double x = fraction + (double) halfWidth - 1.0 - (double) tap;

                double sinc = 1.0;

                if (std::abs (x) > 1.0e-12)
                    sinc = std::sin (juce::MathConstants<double>::pi * twoFc * x)
                           / (juce::MathConstants<double>::pi * twoFc * x);

                // Hann window over +-halfWidth.
                const double window = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * x
                                                             / (double) halfWidth);

                const auto weight = (float) (twoFc * sinc * window);
                row[tap] = weight;
                sum += (double) weight;
            }

            if (std::abs (sum) > 1.0e-12)
            {
                const auto scale = (float) (1.0 / sum);

                for (int tap = 0; tap < taps; ++tap)
                    row[tap] *= scale;
            }
        }
    }

    double cutoff = 0.5;
    int halfWidth = 0;
    int taps = 0;
    std::vector<float> weights;
};

/** Polyphase windowed-sinc resampler.

    Output sample n is the source waveform evaluated at
    `n * sourceRate / destinationRate`; the output length is rounded to the
    nearest frame so the duration matches the source. Samples outside the
    source are treated as silence, which is what a one-shot sampler wants.
*/
juce::AudioBuffer<float> resampleBuffer (const juce::AudioBuffer<float>& source,
                                         double sourceRate,
                                         double destinationRate)
{
    const int numChannels = source.getNumChannels();
    const int sourceFrames = source.getNumSamples();

    if (numChannels <= 0 || sourceFrames <= 0)
        return {};

    if (std::abs (sourceRate - destinationRate) < 1.0e-9)
    {
        juce::AudioBuffer<float> copy (numChannels, sourceFrames);
        copy.makeCopyOf (source);
        return copy;
    }

    const auto roundedLength = std::llround ((double) sourceFrames * destinationRate / sourceRate);
    const int outputFrames = (int) juce::jlimit ((juce::int64) 1,
                                                 (juce::int64) std::numeric_limits<int>::max(),
                                                 (juce::int64) roundedLength);

    juce::AudioBuffer<float> destination (numChannels, outputFrames);
    destination.clear();

    const SincTable table (sourceRate, destinationRate);
    const double step = sourceRate / destinationRate;
    const int taps = table.taps;
    const int halfWidth = table.halfWidth;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* input = source.getReadPointer (channel);
        float* output = destination.getWritePointer (channel);

        for (int n = 0; n < outputFrames; ++n)
        {
            const double position = (double) n * step;
            const double base = std::floor (position);
            const double fraction = position - base;

            double phasePosition = fraction * (double) resamplerPhases;
            int phase = (int) phasePosition;

            if (phase >= resamplerPhases)
            {
                phase = resamplerPhases - 1;
                phasePosition = (double) resamplerPhases;
            }

            const auto blend = (float) (phasePosition - (double) phase);
            const float* row0 = table.weights.data() + (size_t) phase * (size_t) taps;
            const float* row1 = row0 + taps;

            const auto firstInput = (juce::int64) base - halfWidth + 1;

            int firstTap = 0;

            if (firstInput < 0)
                firstTap = (int) juce::jmin ((juce::int64) taps, -firstInput);

            int lastTap = taps;

            if (firstInput + taps > sourceFrames)
                lastTap = (int) juce::jmax ((juce::int64) firstTap, (juce::int64) sourceFrames - firstInput);

            float sum = 0.0f;

            for (int tap = firstTap; tap < lastTap; ++tap)
            {
                const float weight = row0[tap] + blend * (row1[tap] - row0[tap]);
                sum += input[firstInput + tap] * weight;
            }

            output[n] = sum;
        }
    }

    return destination;
}

//==============================================================================

bool isSupportedEngineRate (double rate) noexcept
{
    return std::isfinite (rate) && rate >= 8000.0 && rate <= 384000.0;
}

} // namespace

//==============================================================================
// SampleHandoff

SampleHandoff::~SampleHandoff()
{
    delete installed.exchange (nullptr, std::memory_order_acq_rel);
    delete incoming.exchange (nullptr, std::memory_order_acq_rel);
    collectGarbage();
}

void SampleHandoff::publish (std::unique_ptr<SampleData> sample) noexcept
{
    auto* raw = sample.release();

    // If the audio thread never consumed the previous publication, it is
    // safe to free it here: the exchange is the linearisation point, so the
    // consumer either takes the old pointer or this thread does.
    delete incoming.exchange (raw, std::memory_order_acq_rel);
}

const SampleData* SampleHandoff::poll() noexcept
{
    auto* next = incoming.exchange (nullptr, std::memory_order_acquire);

    if (next == nullptr)
        return nullptr;

    auto* previous = installed.exchange (next, std::memory_order_acq_rel);

    if (previous != nullptr)
        pushRetired (previous);

    return next;
}

void SampleHandoff::pushRetired (const SampleData* sample) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    retireFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        retireSlots[start1] = sample;
        retireFifo.finishedWrite (1);
    }
    else if (size2 > 0)
    {
        retireSlots[start2] = sample;
        retireFifo.finishedWrite (1);
    }
    else
    {
        // Never free on the audio thread. Leaking a sample in this
        // pathological case is the least-bad option, and the counter makes it
        // visible in tests and diagnostics.
        overflowCount.fetch_add (1, std::memory_order_relaxed);
    }
}

void SampleHandoff::collectGarbage() noexcept
{
    const std::lock_guard<std::mutex> lock (collectMutex);

    for (;;)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        retireFifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 <= 0 && size2 <= 0)
            break;

        const auto index = size1 > 0 ? start1 : start2;
        const auto* sample = retireSlots[(size_t) index];
        retireSlots[(size_t) index] = nullptr;
        retireFifo.finishedRead (1);

        delete sample;
    }
}

int SampleHandoff::getNumRetired() const noexcept
{
    return retireFifo.getNumReady();
}

//==============================================================================
// SampleLoader

SampleLoader::SampleLoader()
    : juce::Thread ("ihub-sample-loader")
{
    // The loader is a service: it exists from startup to shutdown even while
    // idle, so the audio thread never has to start a thread.
    startThread (juce::Thread::Priority::low);
}

SampleLoader::~SampleLoader()
{
    signalThreadShouldExit();
    wakeUp.signal();
    stopThread (-1);

    const std::lock_guard<std::mutex> lock (queueMutex);
    jobs.clear();
}

bool SampleLoader::prepare (double newEngineSampleRate) noexcept
{
    if (! isSupportedEngineRate (newEngineSampleRate))
        return false;

    engineSampleRate.store (newEngineSampleRate, std::memory_order_release);
    return true;
}

double SampleLoader::getEngineSampleRate() const noexcept
{
    return engineSampleRate.load (std::memory_order_acquire);
}

std::unique_ptr<SampleData> SampleLoader::decode (const juce::File& file,
                                                  double engineRate,
                                                  juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return nullptr;
    }

    if (! isSupportedEngineRate (engineRate))
    {
        error = "Unsupported engine sample rate";
        return nullptr;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + file.getFileName();
        return nullptr;
    }

    const auto sourceFrames = reader->lengthInSamples;
    const auto sourceRate = reader->sampleRate;

    if (sourceFrames <= 0 || ! std::isfinite (sourceRate) || sourceRate < 1.0)
    {
        error = "Audio file is empty or has an invalid sample rate";
        return nullptr;
    }

    if (sourceFrames > maximumSourceFrames
        || (double) sourceFrames / sourceRate > maximumDurationSeconds)
    {
        error = "Audio file is longer than the "
                + juce::String (maximumDurationSeconds, 0) + " second limit";
        return nullptr;
    }

    const auto sourceChannels = (int) reader->numChannels;
    const int channels = juce::jlimit (1, 2, sourceChannels);

    juce::AudioBuffer<float> decoded (channels, (int) sourceFrames);
    decoded.clear();

    if (! reader->read (&decoded, 0, (int) sourceFrames, 0, true, true))
    {
        error = "Failed to read audio data from " + file.getFileName();
        return nullptr;
    }

    auto destination = std::make_unique<SampleData>();

    destination->audio = resampleBuffer (decoded, sourceRate, engineRate);
    destination->sampleRate = engineRate;
    destination->sourceSampleRate = sourceRate;
    destination->sourceFrames = sourceFrames;
    destination->sourceChannels = sourceChannels;
    destination->name = file.getFileName();
    destination->path = file.getFullPathName();

    if (destination->audio.getNumSamples() <= 0 || destination->audio.getNumChannels() <= 0)
    {
        error = "Resampled audio is empty";
        return nullptr;
    }

    return destination;
}

SampleLoader::Ptr SampleLoader::loadBlocking (const juce::File& file,
                                              double engineSampleRate,
                                              juce::String& error)
{
    error.clear();
    auto decoded = decode (file, engineSampleRate, error);

    if (decoded == nullptr)
        return nullptr;

    return Ptr (std::move (decoded));
}

int SampleLoader::loadAsync (const juce::File& file, SampleHandoff& destination, juce::String& error)
{
    error.clear();

    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return 0;
    }

    const auto requestId = nextRequestId.fetch_add (1, std::memory_order_relaxed);

    pending.fetch_add (1, std::memory_order_acq_rel);

    {
        const std::lock_guard<std::mutex> lock (queueMutex);

        if ((int) jobs.size() >= maximumQueuedJobs)
        {
            pending.fetch_sub (1, std::memory_order_acq_rel);
            error = "Sample loader queue is full";
            return 0;
        }

        Job job;
        job.id = requestId;
        job.file = file;
        job.engineSampleRate = getEngineSampleRate();
        job.destination = &destination;
        jobs.push_back (std::move (job));
    }

    wakeUp.signal();
    return requestId;
}

int SampleLoader::loadAsync (const juce::File& file, juce::String& error)
{
    return loadAsync (file, handoff, error);
}

void SampleLoader::finishJob (int id,
                              const juce::File& file,
                              std::unique_ptr<SampleData> sample,
                              juce::String error,
                              SampleHandoff& destination)
{
    Event event;
    event.requestId = id;
    event.path = file.getFullPathName();
    event.success = sample != nullptr;
    event.error = std::move (error);

    if (sample != nullptr)
        destination.publish (std::move (sample));

    pushEvent (std::move (event));
    pending.fetch_sub (1, std::memory_order_acq_rel);
}

void SampleLoader::pushEvent (Event event)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        events[start1] = std::move (event);
        eventFifo.finishedWrite (1);
    }
    else if (size2 > 0)
    {
        events[start2] = std::move (event);
        eventFifo.finishedWrite (1);
    }
    else
    {
        droppedEvents.fetch_add (1, std::memory_order_relaxed);
    }
}

bool SampleLoader::takeEvent (Event& event)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToRead (1, start1, size1, start2, size2);

    if (size1 <= 0 && size2 <= 0)
        return false;

    const auto index = size1 > 0 ? start1 : start2;
    event = std::move (events[(size_t) index]);
    events[(size_t) index] = Event {};
    eventFifo.finishedRead (1);

    return true;
}

bool SampleLoader::waitForIdle (int timeoutMs)
{
    const auto start = juce::Time::getMillisecondCounterHiRes();

    while (isBusy())
    {
        if (timeoutMs >= 0
            && juce::Time::getMillisecondCounterHiRes() - start >= (double) timeoutMs)
        {
            return false;
        }

        juce::Thread::sleep (1);
    }

    return true;
}

void SampleLoader::run()
{
    while (! threadShouldExit())
    {
        Job job;

        {
            const std::lock_guard<std::mutex> lock (queueMutex);

            if (! jobs.empty())
            {
                job = std::move (jobs.front());
                jobs.pop_front();
            }
        }

        if (job.destination == nullptr)
        {
            wakeUp.wait (100);
            continue;
        }

        juce::String error;
        auto sample = decode (job.file, job.engineSampleRate, error);

        finishJob (job.id, job.file, std::move (sample), std::move (error), *job.destination);
    }
}

} // namespace ihub::sampler
