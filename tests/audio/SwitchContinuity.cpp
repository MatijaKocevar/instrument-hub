// ihub-switch-continuity-test: acceptance check for T034.
//
// Renders the master bus (SlotSwitcher -> Bus -> MasterLimiter) while 50
// randomly timed switches move the armed slot across four sine sources. The
// schedule is seeded from a constant, so the render is deterministic.
//
// Two independent click detectors inspect the rendered master bus:
//
//   1. Sample-to-sample delta. |x[n] - x[n-1]| must never exceed a threshold
//      derived from the largest slew a continuous sum of the sources plus the
//      equal-power ramp can produce. A hard switch leaves a step far above it.
//   2. Spectrum splatter. Each 1024-sample Hann frame (50% overlap) must keep
//      the share of its energy above 1 kHz below -35 dB. A hard step injects
//      broadband energy (measured worst frame: -13.9 dB); the 3 ms equal-power
//      ramp only leaves narrow sidebands and small envelope-corner residue
//      (measured worst frame: -40.6 dB).
//
// The same harness then runs with the ramp disabled (0 ms). Both detectors
// must report detections there, which proves the acceptance check can fail.

#include "audio/Bus.h"
#include "audio/Slot.h"
#include "audio/SlotSwitcher.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

//==============================================================================
namespace
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumChannels = 2;
constexpr int kNumSlots = 4;
constexpr int kSwitchCount = 50;
constexpr float kFadeMilliseconds = 3.0f;

constexpr int kPreRollBlocks = 4;
constexpr int kPreRollSamples = kPreRollBlocks * kBlockSize;
constexpr int kMinimumGapSamples = 16;
constexpr int kMaximumGapSamples = 1600;
constexpr int kTailSamples = 4096;

// Fixed seed: randomised tests must reproduce exactly (plan/TESTING.md).
constexpr std::uint32_t kSeed = 20261031u;

constexpr float kAmplitude = 0.15f;
constexpr std::array<double, kNumSlots> kFrequencies { 180.0, 240.0, 300.0, 360.0 };
constexpr std::array<double, kNumSlots> kInitialPhases { 0.0, 0.7, 1.9, 3.3 };

// Delta threshold. The continuous signal is at most the sum of the active
// sources; the ramp adds at most (pi/2) / fadeSamples of gain change per
// sample per slot. Four slots can overlap while a fade is re-armed mid-fade,
// so the bound is deliberately pessimistic:
//
//   sine slew    = amplitude * 2*pi/sr * sum(frequencies)
//   ramp slew    = 4 * amplitude * (pi/2) / fadeSamples
//
// The threshold below is ~1.8x that bound, leaving room for the exact
// sample-level alignment the bound assumes, while a hard switch (a step of
// up to 2 * amplitude) is several times larger.
constexpr double kDeltaThreshold = 0.05;

// Spectrum splatter threshold: energy above 1 kHz relative to the frame's
// total energy. The 3 ms fade's sidebands and its envelope corners stay below
// -40 dB on this schedule; a hard switch reaches -13.9 dB. -35 dB sits between
// the two with >5 dB of margin on either side.
constexpr double kSpectralCutoffHz = 1000.0;
constexpr int kSpectralFrameSize = 1024;
constexpr double kSpectralThresholdDb = -35.0;
constexpr double kEnergyFloor = 1.0e-30;

int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "ok  : " : "FAIL: ") << what << std::endl;

    if (! condition)
        ++failures;
}

//==============================================================================
/** A phase-continuous sine on every channel. */
class SineSource final : public AudioEngine::SlotSource
{
public:
    SineSource (double frequencyToUse, float amplitudeToUse, double phaseOffset)
        : frequency (frequencyToUse),
          amplitude (amplitudeToUse),
          initialPhase (phaseOffset)
    {
    }

    void prepare (double newSampleRate, int) override
    {
        sampleRate = newSampleRate;
        phase = initialPhase;
        increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    }

    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            const auto value = static_cast<float> (amplitude * std::sin (phase));
            phase += increment;

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.getWritePointer (channel)[frame] = value;
        }
    }

private:
    double frequency;
    float amplitude;
    double initialPhase;

    double sampleRate = 48000.0;
    double phase = 0.0;
    double increment = 0.0;
};

//==============================================================================
struct RunResult
{
    std::vector<float> master;
    std::vector<int> switchSamples;
    std::array<int, kNumSlots> switchesPerSlot {};
    int switchesIssued = 0;
    bool limiterStayedUnity = true;
    double peak = 0.0;
};

RunResult renderRun (float fadeMilliseconds)
{
    RunResult result;

    std::vector<std::unique_ptr<SineSource>> sources;
    std::vector<std::unique_ptr<AudioEngine::Slot>> slots;
    std::vector<AudioEngine::Slot*> slotPointers;

    for (int index = 0; index < kNumSlots; ++index)
    {
        sources.push_back (std::make_unique<SineSource> (kFrequencies[static_cast<size_t> (index)],
                                                        kAmplitude,
                                                        kInitialPhases[static_cast<size_t> (index)]));
        slots.push_back (std::make_unique<AudioEngine::Slot> (index));
        slots.back()->setSource (sources.back().get());
        slotPointers.push_back (slots.back().get());
    }

    AudioEngine::SlotSwitcher switcher (fadeMilliseconds);
    switcher.setSlots (slotPointers);
    switcher.prepare (kSampleRate, kBlockSize, kNumChannels);

    AudioEngine::Bus bus;
    bus.prepare (kSampleRate, kBlockSize, kNumChannels);

    juce::AudioBuffer<float> block (kNumChannels, kBlockSize);
    juce::MidiBuffer midi;

    // One slot is selected before recording; its fade-in starts from silence,
    // so the very first samples are continuous too.
    switcher.armSlot (0);

    for (int preRollBlock = 0; preRollBlock < kPreRollBlocks; ++preRollBlock)
    {
        bus.beginBlock (kBlockSize);

        if (switcher.process (kBlockSize, midi))
            bus.add (switcher.getOutputBuffer(), kBlockSize);

        bus.finish (block, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
            result.master.push_back (block.getSample (0, frame));
    }

    // Deterministic random schedule. Gaps as short as 16 samples force re-arms
    // in the middle of a 3 ms fade, which is the hardest continuity case.
    std::mt19937 rng (kSeed);
    std::uniform_int_distribution<int> gapDistribution (kMinimumGapSamples, kMaximumGapSamples);
    std::uniform_int_distribution<int> slotDistribution (0, kNumSlots - 1);

    std::vector<int> eventPositions;
    eventPositions.reserve (kSwitchCount);

    int position = gapDistribution (rng);

    for (int event = 0; event < kSwitchCount; ++event)
    {
        eventPositions.push_back (position);
        position += gapDistribution (rng);
    }

    const int totalBlocks = (eventPositions.back() + kTailSamples + kBlockSize - 1) / kBlockSize;

    int currentSlot = 0;
    std::size_t nextEvent = 0;

    for (int blockIndex = 0; blockIndex < totalBlocks; ++blockIndex)
    {
        const int blockStart = blockIndex * kBlockSize;

        // At most one request per block: the switcher consumes requests at
        // block boundaries, so every issue below is one audible transition.
        if (nextEvent < eventPositions.size()
            && eventPositions[nextEvent] < blockStart + kBlockSize)
        {
            int target = currentSlot;

            do
            {
                target = slotDistribution (rng);
            }
            while (target == currentSlot);

            switcher.armSlot (target);
            currentSlot = target;

            ++result.switchesIssued;
            ++result.switchesPerSlot[static_cast<size_t> (target)];
            result.switchSamples.push_back (kPreRollSamples + blockStart);
            ++nextEvent;
        }

        bus.beginBlock (kBlockSize);

        if (switcher.process (kBlockSize, midi))
            bus.add (switcher.getOutputBuffer(), kBlockSize);

        bus.finish (block, kBlockSize);

        if (bus.getLimiter().getCurrentGain() < 1.0f)
            result.limiterStayedUnity = false;

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const auto sample = block.getSample (0, frame);
            result.peak = juce::jmax (result.peak, static_cast<double> (std::abs (sample)));
            result.master.push_back (sample);
        }
    }

    return result;
}

//==============================================================================
struct DeltaReport
{
    int detections = 0;
    double maxDelta = 0.0;
    int worstSample = -1;
};

DeltaReport analyseDelta (const std::vector<float>& signal)
{
    DeltaReport report;

    for (std::size_t n = 1; n < signal.size(); ++n)
    {
        const double delta = std::abs (static_cast<double> (signal[n])
                                       - static_cast<double> (signal[n - 1]));

        if (delta > report.maxDelta)
        {
            report.maxDelta = delta;
            report.worstSample = static_cast<int> (n);
        }

        if (delta > kDeltaThreshold)
            ++report.detections;
    }

    return report;
}

//==============================================================================
/** In-place iterative radix-2 Cooley-Tukey FFT. Keeps the test dependency-free. */
void fftInPlace (std::vector<double>& re, std::vector<double>& im)
{
    const std::size_t n = re.size();

    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;

        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }

    for (std::size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = -juce::MathConstants<double>::twoPi
                             / static_cast<double> (length);
        const double wLengthRe = std::cos (angle);
        const double wLengthIm = std::sin (angle);

        for (std::size_t start = 0; start < n; start += length)
        {
            double wRe = 1.0;
            double wIm = 0.0;

            for (std::size_t offset = 0; offset < length / 2; ++offset)
            {
                const std::size_t even = start + offset;
                const std::size_t odd = even + length / 2;

                const double evenRe = re[even];
                const double evenIm = im[even];
                const double oddRe = re[odd] * wRe - im[odd] * wIm;
                const double oddIm = re[odd] * wIm + im[odd] * wRe;

                re[even] = evenRe + oddRe;
                im[even] = evenIm + oddIm;
                re[odd] = evenRe - oddRe;
                im[odd] = evenIm - oddIm;

                const double nextWRe = wRe * wLengthRe - wIm * wLengthIm;
                wIm = wRe * wLengthIm + wIm * wLengthRe;
                wRe = nextWRe;
            }
        }
    }
}

struct SpectralReport
{
    int framesAnalysed = 0;
    int detections = 0;
    double maxSplatterDb = -1000.0;
    int worstFrame = -1;
    std::array<int, 6> countsAbove {}; // -70, -60, -50, -40, -30, -20 dB
};

SpectralReport analyseSpectrum (const std::vector<float>& signal)
{
    SpectralReport report;

    constexpr int frameSize = kSpectralFrameSize;
    constexpr int hop = frameSize / 2;
    constexpr int firstHighBin = static_cast<int> (
        std::ceil (kSpectralCutoffHz * frameSize / kSampleRate));

    std::vector<double> window (frameSize);
    std::vector<double> re (frameSize);
    std::vector<double> im (frameSize);

    for (int i = 0; i < frameSize; ++i)
        window[static_cast<size_t> (i)] = 0.5
            - 0.5 * std::cos (juce::MathConstants<double>::twoPi
                              * static_cast<double> (i)
                              / static_cast<double> (frameSize - 1));

    const int sampleCount = static_cast<int> (signal.size());

    constexpr std::array<double, 6> diagnosticThresholds { -70.0, -60.0, -50.0,
                                                           -40.0, -30.0, -20.0 };

    for (int start = 0; start + frameSize <= sampleCount; start += hop)
    {
        for (int i = 0; i < frameSize; ++i)
        {
            re[static_cast<size_t> (i)] =
                static_cast<double> (signal[static_cast<size_t> (start + i)])
                * window[static_cast<size_t> (i)];
            im[static_cast<size_t> (i)] = 0.0;
        }

        fftInPlace (re, im);

        double totalEnergy = 0.0;
        double highEnergy = 0.0;

        // Skip DC (bin 0): the Hann window's mean is not splatter.
        for (int bin = 1; bin < frameSize / 2; ++bin)
        {
            const double magnitudeSquared = re[static_cast<size_t> (bin)] * re[static_cast<size_t> (bin)]
                                          + im[static_cast<size_t> (bin)] * im[static_cast<size_t> (bin)];

            totalEnergy += magnitudeSquared;

            if (bin >= firstHighBin)
                highEnergy += magnitudeSquared;
        }

        const double splatterDb = 10.0 * std::log10 ((highEnergy + kEnergyFloor)
                                                     / (totalEnergy + kEnergyFloor));

        ++report.framesAnalysed;

        for (std::size_t band = 0; band < diagnosticThresholds.size(); ++band)
            if (splatterDb > diagnosticThresholds[band])
                ++report.countsAbove[band];

        if (splatterDb > report.maxSplatterDb)
        {
            report.maxSplatterDb = splatterDb;
            report.worstFrame = start;
        }

        if (splatterDb > kSpectralThresholdDb)
            ++report.detections;
    }

    return report;
}

//==============================================================================
void runCase (const std::string& label, float fadeMilliseconds, bool expectDetections)
{
    std::cout << "\n-- " << label << " (fade " << fadeMilliseconds << " ms) --" << std::endl;

    const auto rendered = renderRun (fadeMilliseconds);

    check (rendered.switchesIssued == kSwitchCount,
           "50 switches were issued");
    check (static_cast<int> (rendered.switchSamples.size()) == kSwitchCount,
           "50 switches reached the mixer");
    check (rendered.switchesPerSlot[0] > 0
               && rendered.switchesPerSlot[1] > 0
               && rendered.switchesPerSlot[2] > 0
               && rendered.switchesPerSlot[3] > 0,
           "all four slots were selected at least once");
    check (rendered.limiterStayedUnity,
           "the master limiter stayed at unity (linear path, no gain riding)");
    check (rendered.peak < 0.5,
           "peak stays well below the limiter threshold");
    check (static_cast<int> (rendered.master.size())
               >= kPreRollSamples + kTailSamples,
           "the master bus was captured for the whole run");

    const auto delta = analyseDelta (rendered.master);
    const auto spectral = analyseSpectrum (rendered.master);

    std::cout << "      delta threshold: " << kDeltaThreshold
              << ", measured max |x[n]-x[n-1]|: " << delta.maxDelta
              << " at sample " << delta.worstSample << std::endl;
    std::cout << "      delta detections: " << delta.detections << std::endl;
    std::cout << "      spectral threshold: " << kSpectralThresholdDb
              << " dB, measured max splatter: " << spectral.maxSplatterDb
              << " dB at frame " << spectral.worstFrame
              << " (" << spectral.framesAnalysed << " frames)" << std::endl;
    std::cout << "      spectral detections: " << spectral.detections << std::endl;
    std::cout << "      frames above -70/-60/-50/-40/-30/-20 dB:";
    for (auto count : spectral.countsAbove)
        std::cout << ' ' << count;
    std::cout << std::endl;

    if (expectDetections)
    {
        check (delta.detections > 0,
               "the sample-to-sample delta detector fires when the ramp is disabled");
        check (spectral.detections > 0,
               "the spectrum-splatter detector fires when the ramp is disabled");
    }
    else
    {
        check (delta.maxDelta <= kDeltaThreshold,
               "no sample-to-sample delta above the threshold");
        check (delta.detections == 0,
               "delta detector: zero detections across 50 switches");
        check (spectral.maxSplatterDb <= kSpectralThresholdDb,
               "no frame exceeds the splatter threshold");
        check (spectral.detections == 0,
               "spectrum detector: zero detections across 50 switches");
    }
}

} // namespace

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "ihub-switch-continuity-test\n"
              << "  source slots: " << kNumSlots
              << ", switches: " << kSwitchCount
              << ", block: " << kBlockSize
              << ", sample rate: " << kSampleRate
              << ", seed: " << kSeed << std::endl;

    // The acceptance path: the 3 ms equal-power ramp must be inaudible to both
    // detectors across every one of the 50 switches.
    runCase ("ramped switching", kFadeMilliseconds, false);

    // The negative control: with the ramp disabled the same schedule must be
    // caught, otherwise the test above proves nothing.
    runCase ("hard switching (ramp disabled)", 0.0f, true);

    std::cout << (failures == 0 ? "\nPASS" : "\nFAIL") << ": " << failures
              << " failed check(s)" << std::endl;

    return failures == 0 ? 0 : 1;
}
