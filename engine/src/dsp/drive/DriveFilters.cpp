#include "dsp/drive/DriveFilters.h"

#include <algorithm>
#include <cmath>

namespace ihub::dsp {

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double minimumFrequencyHz = 20.0;
constexpr double minimumQ = 0.1f;
constexpr double maximumQ = 10.0f;

constexpr float cabHighPassHz = 85.0f;
constexpr float dcBlockHz = 20.0f;
constexpr float cabLowPassHz = 4800.0f;
constexpr float cabLowMidHz = 120.0f;
constexpr float cabLowMidGainDb = 2.5f;
constexpr float cabLowMidQ = 1.1f;
constexpr float cabPresenceHz = 2600.0f;
constexpr float cabPresenceGainDb = 2.5f;
constexpr float cabPresenceQ = 1.6f;

constexpr float toneMinimumHz = 1400.0f;
constexpr float toneMaximumHz = 11000.0f;
constexpr float toneQ = 0.7f;

float clampFrequency(double sampleRate, float frequencyHz) noexcept {
    const auto maximum = sampleRate * 0.45;
    return static_cast<float>(
        std::clamp(static_cast<double>(frequencyHz), minimumFrequencyHz, maximum));
}

double clampQ(float q) noexcept { return std::clamp(static_cast<double>(q), minimumQ, maximumQ); }
}  // namespace

Biquad DriveFilters::makeHighPass(double rate, float frequencyHz, float q) noexcept {
    const double frequency = clampFrequency(rate, frequencyHz);
    const double w0 = 2.0 * pi * frequency / rate;
    const double cosine = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * clampQ(q));
    const double a0 = 1.0 + alpha;

    Biquad filter;
    filter.b0 = static_cast<float>((1.0 + cosine) * 0.5 / a0);
    filter.b1 = static_cast<float>(-(1.0 + cosine) / a0);
    filter.b2 = filter.b0;
    filter.a1 = static_cast<float>(-2.0 * cosine / a0);
    filter.a2 = static_cast<float>((1.0 - alpha) / a0);
    return filter;
}

Biquad DriveFilters::makeLowPass(double rate, float frequencyHz, float q) noexcept {
    const double frequency = clampFrequency(rate, frequencyHz);
    const double w0 = 2.0 * pi * frequency / rate;
    const double cosine = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * clampQ(q));
    const double a0 = 1.0 + alpha;

    Biquad filter;
    filter.b0 = static_cast<float>((1.0 - cosine) * 0.5 / a0);
    filter.b1 = static_cast<float>((1.0 - cosine) / a0);
    filter.b2 = filter.b0;
    filter.a1 = static_cast<float>(-2.0 * cosine / a0);
    filter.a2 = static_cast<float>((1.0 - alpha) / a0);
    return filter;
}

Biquad DriveFilters::makePeak(double rate, float frequencyHz, float gainDb, float q) noexcept {
    const double frequency = clampFrequency(rate, frequencyHz);
    const double w0 = 2.0 * pi * frequency / rate;
    const double cosine = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * clampQ(q));
    const double a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const double a0 = 1.0 + alpha / a;

    Biquad filter;
    filter.b0 = static_cast<float>((1.0 + alpha * a) / a0);
    filter.b1 = static_cast<float>(-2.0 * cosine / a0);
    filter.b2 = static_cast<float>((1.0 - alpha * a) / a0);
    filter.a1 = filter.b1;
    filter.a2 = static_cast<float>((1.0 - alpha / a) / a0);
    return filter;
}

void DriveFilters::prepare(double newSampleRate, int numChannels) {
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    preparedChannels = std::clamp(numChannels, 1, 2);

    const auto highPass = makeHighPass(sampleRate, cabHighPassHz, 0.7f);
    const auto lowPass = makeLowPass(sampleRate, cabLowPassHz, 0.7f);
    const auto lowMid = makePeak(sampleRate, cabLowMidHz, cabLowMidGainDb, cabLowMidQ);
    const auto presence = makePeak(sampleRate, cabPresenceHz, cabPresenceGainDb, cabPresenceQ);
    const auto dcBlock = makeHighPass(sampleRate, dcBlockHz, 0.7f);

    for (auto& channel : channels) {
        channel = Channel{};
        channel.dcBlock.setCoefficients(dcBlock);
        channel.cabHighPass.setCoefficients(highPass);
        channel.cabLowPass.setCoefficients(lowPass);
        channel.cabLowMid.setCoefficients(lowMid);
        channel.cabPresence.setCoefficients(presence);
    }

    toneValue = -1.0f;
}

void DriveFilters::reset() noexcept {
    for (auto& channel : channels) {
        channel.highPass.reset();
        channel.mid.reset();
        channel.presence.reset();
        channel.dcBlock.reset();
        channel.tone.reset();
        channel.cabHighPass.reset();
        channel.cabLowPass.reset();
        channel.cabLowMid.reset();
        channel.cabPresence.reset();
    }
}

void DriveFilters::setVoicing(const DriveVoicing& newVoicing) noexcept {
    voicing = newVoicing;

    const auto highPass = makeHighPass(sampleRate, voicing.highPassHz, 0.7f);
    const auto mid = makePeak(sampleRate, voicing.midHz, voicing.midGainDb, voicing.midQ);
    const auto presence = makePeak(sampleRate, voicing.presenceHz, voicing.presenceGainDb, 0.8f);

    for (auto& channel : channels) {
        channel.highPass.setCoefficients(highPass);
        channel.mid.setCoefficients(mid);
        channel.presence.setCoefficients(presence);
    }
}

void DriveFilters::setTone(float tone) noexcept {
    const float clamped = std::clamp(tone, 0.0f, 1.0f);

    if (std::abs(clamped - toneValue) < 0.0005f) return;

    toneValue = clamped;

    const float cutoff = toneMinimumHz * std::pow(toneMaximumHz / toneMinimumHz, clamped);
    const auto lowPass = makeLowPass(sampleRate, cutoff, toneQ);

    for (auto& channel : channels) channel.tone.setCoefficients(lowPass);
}

DriveFilters::Channel& DriveFilters::channelFor(int channel) noexcept {
    return channels[static_cast<std::size_t>(std::clamp(channel, 0, preparedChannels - 1))];
}

float DriveFilters::processPre(int channel, float sample) noexcept {
    auto& filters = channelFor(channel);

    sample = filters.highPass.process(sample);
    sample = filters.mid.process(sample);
    return filters.presence.process(sample);
}

float DriveFilters::processTone(int channel, float sample) noexcept {
    return channelFor(channel).tone.process(sample);
}

float DriveFilters::processDc(int channel, float sample) noexcept {
    return channelFor(channel).dcBlock.process(sample);
}

float DriveFilters::processCab(int channel, float sample) noexcept {
    auto& filters = channelFor(channel);

    sample = filters.cabHighPass.process(sample);
    sample = filters.cabLowPass.process(sample);
    sample = filters.cabLowMid.process(sample);
    return filters.cabPresence.process(sample);
}

}  // namespace ihub::dsp
