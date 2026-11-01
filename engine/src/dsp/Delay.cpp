#include "Delay.h"

#include <algorithm>
#include <cmath>

namespace ihub::dsp
{

namespace
{
constexpr int maxSnapshotReadAttempts = 3;

constexpr double crossfadeSeconds = 0.02;
}

Delay::Delay()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    applied = computePublished();
    publishSettings();
    appliedVersion = snapshot.version.load (std::memory_order_acquire);
}

int Delay::delaySamplesForDivision (DelayDivision division, double bpm,
                                    double sampleRate) noexcept
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto secondsPerBeat = 60.0 / clampBpm (bpm);
    const auto seconds = juce::jlimit (1.0 / rate, maxDelaySeconds,
                                       secondsPerBeat * divisionInBeats (division));

    const auto maxSamples = static_cast<int> (std::ceil (maxDelaySeconds * rate));

    return juce::jlimit (1, juce::jmax (1, maxSamples),
                         static_cast<int> (std::llround (seconds * rate)));
}

int Delay::delaySamplesForMs (float milliseconds, double sampleRate) noexcept
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto clampedMs = juce::jlimit (minTimeMs, maxTimeMs, milliseconds);
    const auto maxSamples = static_cast<int> (std::ceil (maxDelaySeconds * rate));

    return juce::jlimit (1, juce::jmax (1, maxSamples),
                         static_cast<int> (std::llround (static_cast<double> (clampedMs)
                                                         * 0.001 * rate)));
}

void Delay::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    const auto numChannels = juce::jmax (1, getTotalNumInputChannels());
    const auto lineSize =
        static_cast<int> (std::ceil (maxDelaySeconds * currentSampleRate)) + 2;

    delayBuffer.setSize (numChannels, lineSize, false, true, false);
    dampingState.assign (static_cast<std::size_t> (numChannels), 0.0f);

    reset();

    applied = computePublished();
    publishSettings();
    appliedVersion = snapshot.version.load (std::memory_order_acquire);
    activeDelaySamples = applied.delaySamples;
    previousDelaySamples = activeDelaySamples;
    crossfadeRemaining = 0;

    prepared = true;

    setLatencySamples (0);
}

void Delay::releaseResources()
{
    prepared = false;
}

void Delay::reset()
{
    delayBuffer.clear();
    writeIndex = 0;

    std::fill (dampingState.begin(), dampingState.end(), 0.0f);

    const auto current = computePublished().delaySamples;
    activeDelaySamples = current;
    previousDelaySamples = current;
    crossfadeRemaining = 0;
    crossfadeLength = 0;
}

void Delay::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    const int lineSize = delayBuffer.getNumSamples();

    applied.delaySamples = juce::jlimit (1, juce::jmax (1, lineSize), applied.delaySamples);

    if (applied.delaySamples != activeDelaySamples)
    {
        previousDelaySamples = activeDelaySamples;
        activeDelaySamples = applied.delaySamples;
        crossfadeLength = juce::jmax (1, static_cast<int> (crossfadeSeconds
                                                         * currentSampleRate));
        crossfadeRemaining = crossfadeLength;
    }

    const bool bypassed = ! enabled.load (std::memory_order_relaxed);
    const auto mix = applied.mix;
    const float dryGain = 1.0f - mix;

    auto* const channels = buffer.getArrayOfWritePointers();
    auto* const line = delayBuffer.getArrayOfWritePointers();

    const int channelsToProcess = juce::jmin (numChannels, delayBuffer.getNumChannels());
    const int active = juce::jlimit (1, juce::jmax (1, lineSize), activeDelaySamples);
    const int previous = juce::jlimit (1, juce::jmax (1, lineSize), previousDelaySamples);

    int write = writeIndex;
    int fade = crossfadeRemaining;

    for (int i = 0; i < numSamples; ++i)
    {
        const float fadeGain = fade > 0
                                   ? 1.0f - static_cast<float> (fade)
                                                 / static_cast<float> (juce::jmax (1, crossfadeLength))
                                   : 1.0f;

        int readActive = write - active;

        if (readActive < 0)
            readActive += lineSize;

        int readPrevious = write - previous;

        if (readPrevious < 0)
            readPrevious += lineSize;

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            const float dry = channels[channel][i];

            float wet = line[channel][readActive];

            if (fade > 0 && previous != active)
                wet = line[channel][readPrevious] * (1.0f - fadeGain) + wet * fadeGain;

            float& damped = dampingState[static_cast<std::size_t> (channel)];
            damped += (wet - damped) * applied.dampingCoeff;

            line[channel][write] = dry + damped * applied.feedback;

            if (bypassed)
                continue;

            if (mix >= 1.0f)
                channels[channel][i] = wet;
            else if (mix > 0.0f)
                channels[channel][i] = dry * dryGain + wet * mix;
        }

        if (fade > 0)
            --fade;

        if (++write >= lineSize)
            write = 0;
    }

    writeIndex = write;
    crossfadeRemaining = fade;

    for (int channel = channelsToProcess; channel < numChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

bool Delay::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto inputs = layouts.getMainInputChannelSet();
    const auto outputs = layouts.getMainOutputChannelSet();

    if (inputs != outputs)
        return false;

    return inputs == juce::AudioChannelSet::mono()
           || inputs == juce::AudioChannelSet::stereo();
}

DelaySettings Delay::clampSettings (const DelaySettings& input) noexcept
{
    DelaySettings s = input;

    if (auto division = static_cast<int> (s.division);
        division < 0 || division >= static_cast<int> (DelayDivision::numDivisions))
        s.division = DelayDivision::quarter;

    s.timeMs = juce::jlimit (minTimeMs, maxTimeMs, s.timeMs);
    s.feedback = juce::jlimit (minFeedback, maxFeedback, s.feedback);
    s.mix = juce::jlimit (0.0f, 1.0f, s.mix);
    s.dampingHz = juce::jlimit (minDampingHz, maxDampingHz, s.dampingHz);

    return s;
}

double Delay::clampBpm (double bpm) noexcept
{
    if (! std::isfinite (bpm))
        return 120.0;

    return juce::jlimit (minBpm, maxBpm, bpm);
}

Delay::Published Delay::computePublished() const noexcept
{
    const auto clamped = clampSettings (settings);
    const auto bpm = clampBpm (transportBpm.load (std::memory_order_relaxed));

    Published p;

    p.delaySamples = clamped.tempoSync
                         ? delaySamplesForDivision (clamped.division, bpm, currentSampleRate)
                         : delaySamplesForMs (clamped.timeMs, currentSampleRate);

    p.feedback = clamped.feedback;
    p.mix = clamped.mix;

    const auto cutoff = juce::jlimit (minDampingHz,
                                      juce::jmin (maxDampingHz,
                                                  0.45f * static_cast<float> (currentSampleRate)),
                                      clamped.dampingHz);
    p.dampingCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * cutoff
                                      / static_cast<float> (currentSampleRate));

    return p;
}

void Delay::setSettings (const DelaySettings& newSettings)
{
    settings = clampSettings (newSettings);
    publishSettings();
}

DelaySettings Delay::getSettings() const noexcept
{
    return settings;
}

void Delay::setTempoSync (bool shouldSync)
{
    auto s = settings;
    s.tempoSync = shouldSync;
    setSettings (s);
}

void Delay::setDivision (DelayDivision division)
{
    auto s = settings;
    s.division = division;
    setSettings (s);
}

void Delay::setTimeMs (float milliseconds)
{
    auto s = settings;
    s.timeMs = milliseconds;
    setSettings (s);
}

void Delay::setFeedback (float feedback)
{
    auto s = settings;
    s.feedback = feedback;
    setSettings (s);
}

void Delay::setMix (float mix)
{
    auto s = settings;
    s.mix = mix;
    setSettings (s);
}

void Delay::setDampingHz (float dampingHz)
{
    auto s = settings;
    s.dampingHz = dampingHz;
    setSettings (s);
}

void Delay::setTransport (const DelayTransport& transport)
{
    transportBpm.store (clampBpm (transport.bpm), std::memory_order_relaxed);
    transportPpq.store (std::isfinite (transport.ppq) ? transport.ppq : 0.0,
                        std::memory_order_relaxed);
    transportPlaying.store (transport.playing, std::memory_order_relaxed);

    publishSettings();
}

DelayTransport Delay::getTransport() const noexcept
{
    DelayTransport transport;
    transport.bpm = transportBpm.load (std::memory_order_relaxed);
    transport.ppq = transportPpq.load (std::memory_order_relaxed);
    transport.playing = transportPlaying.load (std::memory_order_relaxed);
    return transport;
}

void Delay::setEnabled (bool shouldBeEnabled)
{
    enabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

bool Delay::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

int Delay::getDelaySamples() const noexcept
{
    return reportedDelaySamples.load (std::memory_order_relaxed);
}

double Delay::getDelaySeconds() const noexcept
{
    const auto rate = currentSampleRate > 0.0 ? currentSampleRate : 48000.0;
    return static_cast<double> (getDelaySamples()) / rate;
}

double Delay::getDelayInPpq() const noexcept
{
    const auto bpm = clampBpm (transportBpm.load (std::memory_order_relaxed));
    return getDelaySeconds() * bpm / 60.0;
}

double Delay::getNextRepeatPpq() const noexcept
{
    return transportPpq.load (std::memory_order_relaxed) + getDelayInPpq();
}

void Delay::publishSettings() noexcept
{
    const auto publishedValues = computePublished();

    snapshot.version.fetch_add (1, std::memory_order_acq_rel);
    snapshot.values.store (publishedValues);
    snapshot.version.fetch_add (1, std::memory_order_release);

    reportedDelaySamples.store (publishedValues.delaySamples, std::memory_order_relaxed);
}

bool Delay::readSnapshot (Published& destination) const noexcept
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

void Delay::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream (destData, false);

    stream.writeInt (1);
    stream.writeBool (isEnabled());
    stream.writeBool (settings.tempoSync);
    stream.writeInt (static_cast<int> (settings.division));
    stream.writeFloat (settings.timeMs);
    stream.writeFloat (settings.feedback);
    stream.writeFloat (settings.mix);
    stream.writeFloat (settings.dampingHz);
}

void Delay::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream (data, static_cast<std::size_t> (sizeInBytes), false);

    if (stream.readInt() != 1)
        return;

    constexpr int expectedBytes = 4 + 2 + 4 + 4 * 4;

    if (sizeInBytes < expectedBytes)
        return;

    const auto shouldBeEnabled = stream.readBool();

    DelaySettings restored;
    restored.tempoSync = stream.readBool();
    restored.division = static_cast<DelayDivision> (stream.readInt());
    restored.timeMs = stream.readFloat();
    restored.feedback = stream.readFloat();
    restored.mix = stream.readFloat();
    restored.dampingHz = stream.readFloat();

    setSettings (restored);
    setEnabled (shouldBeEnabled);
}

}
