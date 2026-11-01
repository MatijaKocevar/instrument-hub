#include "Compressor.h"

#include <cmath>

namespace ihub::dsp
{

namespace
{
constexpr int maxSnapshotReadAttempts = 3;

constexpr float detectorFloorDb = -120.0f;
}

Compressor::Compressor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    applied = computePublished();
    publishSettings();
    appliedVersion = snapshot.version.load (std::memory_order_acquire);
}

void Compressor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    const auto requestedSamples =
        std::lround (static_cast<double> (lookaheadMs.load (std::memory_order_relaxed))
                     * 0.001 * currentSampleRate);

    const auto maxSamples =
        static_cast<int> (std::ceil (maxLookaheadMs * 0.001 * currentSampleRate));

    lookaheadSamples = juce::jlimit (0, maxSamples, static_cast<int> (requestedSamples));

    const auto numChannels = juce::jmax (1, getTotalNumInputChannels());

    delayBuffer.setSize (numChannels, juce::jmax (1, lookaheadSamples), false, true, false);

    reset();

    applied = computePublished();
    publishSettings();
    appliedVersion = snapshot.version.load (std::memory_order_acquire);

    prepared = true;

    setLatencySamples (lookaheadSamples);
}

void Compressor::releaseResources()
{
    prepared = false;
}

void Compressor::reset()
{
    delayBuffer.clear();
    delayIndex = 0;
    detectorPeak = 0.0f;
    gainReductionDb = 0.0f;
    reportedGainReductionDb.store (0.0f, std::memory_order_relaxed);
}

void Compressor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    if (! prepared)
    {
        buffer.clear();
        return;
    }

    const auto version = snapshot.version.load (std::memory_order_acquire);

    if (version != appliedVersion && (version & 1u) == 0u)
    {
        Published pending;

        if (readSnapshot (pending))
        {
            applied = pending;
            appliedVersion = version;
        }
    }

    const bool bypassed = ! enabled.load (std::memory_order_relaxed);

    auto* const channels = buffer.getArrayOfWritePointers();
    const int channelsToProcess = juce::jmin (numChannels, delayBuffer.getNumChannels());

    if (lookaheadSamples == 0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;

            for (int channel = 0; channel < channelsToProcess; ++channel)
                peak = juce::jmax (peak, std::abs (channels[channel][i]));

            const auto gain = advanceGain (peak, applied);

            if (! bypassed)
                for (int channel = 0; channel < channelsToProcess; ++channel)
                    channels[channel][i] *= gain;
        }
    }
    else
    {
        auto* const delay = delayBuffer.getArrayOfWritePointers();
        int index = delayIndex;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;

            for (int channel = 0; channel < channelsToProcess; ++channel)
                peak = juce::jmax (peak, std::abs (channels[channel][i]));

            const auto gain = advanceGain (peak, applied);

            for (int channel = 0; channel < channelsToProcess; ++channel)
            {
                auto& slot = delay[channel][index];
                const auto delayed = slot;

                slot = channels[channel][i];
                channels[channel][i] = bypassed ? delayed : delayed * gain;
            }

            if (++index >= lookaheadSamples)
                index = 0;
        }

        delayIndex = index;
    }

    for (int channel = channelsToProcess; channel < numChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    reportedGainReductionDb.store (bypassed ? 0.0f : gainReductionDb,
                                   std::memory_order_relaxed);
}

bool Compressor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inputs = layouts.getMainInputChannelSet();
    const auto outputs = layouts.getMainOutputChannelSet();

    if (inputs != outputs)
        return false;

    return inputs == juce::AudioChannelSet::mono()
           || inputs == juce::AudioChannelSet::stereo();
}

float Compressor::advanceGain (float inputPeak, const Published& published) noexcept
{
    if (inputPeak > detectorPeak)
        detectorPeak = inputPeak;
    else
        detectorPeak *= published.peakDecay;

    const auto levelDb = juce::Decibels::gainToDecibels (detectorPeak, detectorFloorDb);

    const auto targetDb = levelDb > published.thresholdDb
                              ? (levelDb - published.thresholdDb) * published.slope
                              : 0.0f;

    const auto coefficient = targetDb > gainReductionDb ? published.attackCoeff
                                                        : published.releaseCoeff;

    gainReductionDb += (targetDb - gainReductionDb) * coefficient;

    if (gainReductionDb < 0.0f)
        gainReductionDb = 0.0f;

    return juce::Decibels::decibelsToGain (-gainReductionDb) * published.makeupGain;
}

CompressorSettings Compressor::clampSettings (const CompressorSettings& input) noexcept
{
    CompressorSettings s = input;

    s.thresholdDb = juce::jlimit (minThresholdDb, maxThresholdDb, s.thresholdDb);
    s.ratio = juce::jlimit (minRatio, maxRatio, s.ratio);
    s.attackMs = juce::jlimit (minAttackMs, maxAttackMs, s.attackMs);
    s.releaseMs = juce::jlimit (minReleaseMs, maxReleaseMs, s.releaseMs);
    s.makeupDb = juce::jlimit (minMakeupDb, maxMakeupDb, s.makeupDb);

    return s;
}

float Compressor::onePoleCoefficient (float timeMs, double sampleRate) noexcept
{
    const auto seconds = juce::jmax (1.0e-6f, timeMs * 0.001f);

    return 1.0f
           - std::exp (-1.0f / (seconds * static_cast<float> (sampleRate)));
}

Compressor::Published Compressor::computePublished() const noexcept
{
    const auto clamped = clampSettings (settings);

    Published p;
    p.thresholdDb = clamped.thresholdDb;
    p.slope = 1.0f - 1.0f / clamped.ratio;
    p.attackCoeff = onePoleCoefficient (clamped.attackMs, currentSampleRate);
    p.releaseCoeff = onePoleCoefficient (clamped.releaseMs, currentSampleRate);
    p.peakDecay = 1.0f - p.releaseCoeff;
    p.makeupGain = juce::Decibels::decibelsToGain (clamped.makeupDb);

    return p;
}

void Compressor::setSettings (const CompressorSettings& newSettings)
{
    settings = clampSettings (newSettings);
    publishSettings();
}

CompressorSettings Compressor::getSettings() const noexcept
{
    return settings;
}

void Compressor::setThresholdDb (float value)
{
    auto s = settings;
    s.thresholdDb = value;
    setSettings (s);
}

void Compressor::setRatio (float value)
{
    auto s = settings;
    s.ratio = value;
    setSettings (s);
}

void Compressor::setAttackMs (float value)
{
    auto s = settings;
    s.attackMs = value;
    setSettings (s);
}

void Compressor::setReleaseMs (float value)
{
    auto s = settings;
    s.releaseMs = value;
    setSettings (s);
}

void Compressor::setMakeupDb (float value)
{
    auto s = settings;
    s.makeupDb = value;
    setSettings (s);
}

void Compressor::setEnabled (bool shouldBeEnabled)
{
    enabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

bool Compressor::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void Compressor::setLookaheadMs (float milliseconds)
{
    lookaheadMs.store (juce::jlimit (minLookaheadMs, maxLookaheadMs, milliseconds),
                       std::memory_order_relaxed);
}

float Compressor::getLookaheadMs() const noexcept
{
    return lookaheadMs.load (std::memory_order_relaxed);
}

float Compressor::getGainReductionDb() const noexcept
{
    return reportedGainReductionDb.load (std::memory_order_relaxed);
}

void Compressor::publishSettings() noexcept
{
    const auto publishedValues = computePublished();

    snapshot.version.fetch_add (1, std::memory_order_acq_rel);
    snapshot.values.store (publishedValues);
    snapshot.version.fetch_add (1, std::memory_order_release);
}

bool Compressor::readSnapshot (Published& destination) const noexcept
{
    for (int attempt = 0; attempt < maxSnapshotReadAttempts; ++attempt)
    {
        const auto before = snapshot.version.load (std::memory_order_acquire);

        if ((before & 1u) != 0u)
            continue;

        destination = snapshot.values.load();

        std::atomic_thread_fence (std::memory_order_acquire);

        const auto after = snapshot.version.load (std::memory_order_acquire);

        if (before == after)
            return true;
    }

    return false;
}

void Compressor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream (destData, false);

    stream.writeInt (1);
    stream.writeBool (isEnabled());
    stream.writeFloat (settings.thresholdDb);
    stream.writeFloat (settings.ratio);
    stream.writeFloat (settings.attackMs);
    stream.writeFloat (settings.releaseMs);
    stream.writeFloat (settings.makeupDb);
    stream.writeFloat (getLookaheadMs());
}

void Compressor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream (data, static_cast<std::size_t> (sizeInBytes), false);

    if (stream.readInt() != 1)
        return;

    constexpr int expectedBytes = 4 + 1 + 6 * 4;

    if (sizeInBytes < expectedBytes)
        return;

    const auto shouldBeEnabled = stream.readBool();

    CompressorSettings restored;
    restored.thresholdDb = stream.readFloat();
    restored.ratio = stream.readFloat();
    restored.attackMs = stream.readFloat();
    restored.releaseMs = stream.readFloat();
    restored.makeupDb = stream.readFloat();
    const auto restoredLookaheadMs = stream.readFloat();

    setSettings (restored);
    setEnabled (shouldBeEnabled);
    setLookaheadMs (restoredLookaheadMs);
}

}
