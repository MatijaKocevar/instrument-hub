#include "Eq.h"

#include <cmath>
#include <complex>

namespace ihub::dsp {

namespace {
constexpr int maxSnapshotReadAttempts = 3;

constexpr float flatGainEpsilonDb = 1.0e-6f;
}  // namespace

Eq::Eq()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    bandSettings[0].type = EqBandType::lowShelf;
    bandSettings[0].frequencyHz = 100.0f;
    bandSettings[0].q = 0.7071068f;

    bandSettings[1].type = EqBandType::peak;
    bandSettings[1].frequencyHz = 1000.0f;
    bandSettings[1].q = 1.0f;

    bandSettings[2].type = EqBandType::highShelf;
    bandSettings[2].frequencyHz = 10000.0f;
    bandSettings[2].q = 0.7071068f;
}

void Eq::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    prepared = true;

    const auto numChannels = juce::jmax(1, getTotalNumInputChannels());

    channelFilters.resize(static_cast<std::size_t>(numChannels));

    for (int band = 0; band < numBands; ++band) {
        bandCoefficients[(std::size_t)band] =
            makeJuceCoefficients(designBand(bandSettings[(std::size_t)band], currentSampleRate));

        for (auto& channel : channelFilters) {
            auto& filter = channel[(std::size_t)band];
            filter.coefficients = bandCoefficients[(std::size_t)band];
            filter.reset();
        }
    }

    publishSettings();
    appliedVersion = snapshot.version.load(std::memory_order_acquire);
}

void Eq::releaseResources() { prepared = false; }

void Eq::reset() {
    for (auto& channel : channelFilters)
        for (auto& filter : channel) filter.reset();
}

void Eq::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    const juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0) return;

    if (!prepared) {
        buffer.clear();
        return;
    }

    if (!enabled.load(std::memory_order_relaxed)) return;

    const auto version = snapshot.version.load(std::memory_order_acquire);

    if (version != appliedVersion && (version & 1u) == 0u) {
        std::array<BiquadCoefficients, numBands> pending;

        if (readSnapshot(pending)) {
            applySnapshot(pending);
            appliedVersion = version;
        }
    }

    auto wholeBlock = juce::dsp::AudioBlock<float>(buffer);
    const auto channelsToProcess = juce::jmin(numChannels, static_cast<int>(channelFilters.size()));

    for (int channel = 0; channel < channelsToProcess; ++channel) {
        auto channelBlock = wholeBlock.getSingleChannelBlock((std::size_t)channel);

        for (auto& filter : channelFilters[(std::size_t)channel])
            filter.process(juce::dsp::ProcessContextReplacing<float>(channelBlock));
    }

    for (int channel = channelsToProcess; channel < numChannels; ++channel)
        buffer.clear(channel, 0, numSamples);
}

bool Eq::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto inputs = layouts.getMainInputChannelSet();
    const auto outputs = layouts.getMainOutputChannelSet();

    if (inputs != outputs) return false;

    return inputs == juce::AudioChannelSet::mono() || inputs == juce::AudioChannelSet::stereo();
}

void Eq::setBand(int index, const EqBandSettings& settings) {
    if (!juce::isPositiveAndBelow(index, numBands)) return;

    auto clamped = settings;

    const auto nyquistLimit = static_cast<float>(currentSampleRate * 0.5);
    clamped.frequencyHz = juce::jlimit(
        minFrequencyHz, juce::jmax(minFrequencyHz, juce::jmin(maxFrequencyHz, nyquistLimit)),
        clamped.frequencyHz);
    clamped.q = juce::jlimit(minQ, maxQ, clamped.q);
    clamped.gainDb = juce::jlimit(-maxGainDb, maxGainDb, clamped.gainDb);

    bandSettings[(std::size_t)index] = clamped;
    publishSettings();
}

EqBandSettings Eq::getBand(int index) const noexcept {
    if (!juce::isPositiveAndBelow(index, numBands)) return {};

    return bandSettings[(std::size_t)index];
}

void Eq::setAllBands(const std::array<EqBandSettings, numBands>& settings) {
    for (int band = 0; band < numBands; ++band) {
        auto clamped = settings[(std::size_t)band];

        const auto nyquistLimit = static_cast<float>(currentSampleRate * 0.5);
        clamped.frequencyHz = juce::jlimit(
            minFrequencyHz, juce::jmax(minFrequencyHz, juce::jmin(maxFrequencyHz, nyquistLimit)),
            clamped.frequencyHz);
        clamped.q = juce::jlimit(minQ, maxQ, clamped.q);
        clamped.gainDb = juce::jlimit(-maxGainDb, maxGainDb, clamped.gainDb);

        bandSettings[(std::size_t)band] = clamped;
    }

    publishSettings();
}

std::array<EqBandSettings, Eq::numBands> Eq::getAllBands() const noexcept { return bandSettings; }

void Eq::setEnabled(bool shouldBeEnabled) {
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

bool Eq::isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }

double Eq::getMagnitudeDbAt(double frequencyHz) const noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return 0.0;

    if (frequencyHz <= 0.0 || frequencyHz >= currentSampleRate * 0.5) return -300.0;

    std::array<BiquadCoefficients, numBands> bands;

    if (!readSnapshot(bands)) return 0.0;

    const auto omega = juce::MathConstants<double>::twoPi * frequencyHz / currentSampleRate;
    const auto z1 = std::polar(1.0, -omega);
    const auto z2 = z1 * z1;

    double magnitude = 1.0;

    for (const auto& band : bands) {
        const auto numerator = std::complex<double>(band.b0, 0.0) +
                               static_cast<double>(band.b1) * z1 +
                               static_cast<double>(band.b2) * z2;
        const auto denominator = std::complex<double>(1.0, 0.0) +
                                 static_cast<double>(band.a1) * z1 +
                                 static_cast<double>(band.a2) * z2;

        const auto denominatorMagnitude = std::abs(denominator);

        if (denominatorMagnitude > 0.0) magnitude *= std::abs(numerator) / denominatorMagnitude;
    }

    return magnitude > 0.0 ? juce::Decibels::gainToDecibels(magnitude, -300.0) : -300.0;
}

Eq::BiquadCoefficients Eq::designBand(const EqBandSettings& settings, double sampleRate) noexcept {
    if (!settings.enabled || std::abs(settings.gainDb) < flatGainEpsilonDb || sampleRate <= 0.0)
        return {};

    const auto frequency = static_cast<float>(
        juce::jlimit(2.0, sampleRate * 0.49, static_cast<double>(settings.frequencyHz)));
    const auto q = juce::jlimit(minQ, maxQ, settings.q);
    const auto gain = juce::Decibels::decibelsToGain(settings.gainDb);

    std::array<float, 6> raw{};

    switch (settings.type) {
        case EqBandType::lowShelf:
            raw = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(sampleRate, frequency, q,
                                                                         gain);
            break;

        case EqBandType::peak:
            raw = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(sampleRate, frequency, q,
                                                                           gain);
            break;

        case EqBandType::highShelf:
            raw = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(sampleRate, frequency, q,
                                                                          gain);
            break;
    }

    const auto a0 = raw[3];

    if (std::abs(a0) < 1.0e-12f) return {};

    const auto inverseA0 = 1.0f / a0;

    return {raw[0] * inverseA0, raw[1] * inverseA0, raw[2] * inverseA0, raw[4] * inverseA0,
            raw[5] * inverseA0};
}

juce::dsp::IIR::Coefficients<float>::Ptr Eq::makeJuceCoefficients(
    const BiquadCoefficients& coefficients) {
    return new juce::dsp::IIR::Coefficients<float>(
        coefficients.b0, coefficients.b1, coefficients.b2, 1.0f, coefficients.a1, coefficients.a2);
}

void Eq::publishSettings() noexcept {
    std::array<BiquadCoefficients, numBands> designed;

    for (int band = 0; band < numBands; ++band)
        designed[(std::size_t)band] =
            designBand(bandSettings[(std::size_t)band], currentSampleRate);

    snapshot.version.fetch_add(1, std::memory_order_acq_rel);

    for (int band = 0; band < numBands; ++band)
        snapshot.bands[(std::size_t)band].store(designed[(std::size_t)band]);

    snapshot.version.fetch_add(1, std::memory_order_release);
}

bool Eq::readSnapshot(std::array<BiquadCoefficients, numBands>& destination) const noexcept {
    for (int attempt = 0; attempt < maxSnapshotReadAttempts; ++attempt) {
        const auto before = snapshot.version.load(std::memory_order_acquire);

        if ((before & 1u) != 0u) continue;

        for (int band = 0; band < numBands; ++band)
            destination[(std::size_t)band] = snapshot.bands[(std::size_t)band].load();

        std::atomic_thread_fence(std::memory_order_acquire);

        const auto after = snapshot.version.load(std::memory_order_acquire);

        if (before == after) return true;
    }

    return false;
}

void Eq::applySnapshot(const std::array<BiquadCoefficients, numBands>& coefficients) noexcept {
    for (int band = 0; band < numBands; ++band) {
        auto& target = bandCoefficients[(std::size_t)band];

        if (target == nullptr) continue;

        auto* raw = target->getRawCoefficients();

        if (target->getFilterOrder() != 2 || raw == nullptr) continue;

        const auto& source = coefficients[(std::size_t)band];

        raw[0] = source.b0;
        raw[1] = source.b1;
        raw[2] = source.b2;
        raw[3] = source.a1;
        raw[4] = source.a2;
    }
}

void Eq::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream stream(destData, false);

    stream.writeInt(1);
    stream.writeBool(isEnabled());

    for (const auto& band : bandSettings) {
        stream.writeInt(static_cast<int>(band.type));
        stream.writeFloat(band.frequencyHz);
        stream.writeFloat(band.gainDb);
        stream.writeFloat(band.q);
        stream.writeBool(band.enabled);
    }
}

void Eq::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) return;

    juce::MemoryInputStream stream(data, static_cast<std::size_t>(sizeInBytes), false);

    if (stream.readInt() != 1) return;

    constexpr int expectedBytes = 4 + 1 + numBands * (4 + 4 + 4 + 4 + 1);

    if (sizeInBytes < expectedBytes) return;

    const auto shouldBeEnabled = stream.readBool();

    std::array<EqBandSettings, numBands> restored;

    for (auto& band : restored) {
        const auto type = stream.readInt();

        if (!juce::isPositiveAndBelow(type, 3)) return;

        band.type = static_cast<EqBandType>(type);
        band.frequencyHz = stream.readFloat();
        band.gainDb = stream.readFloat();
        band.q = stream.readFloat();
        band.enabled = stream.readBool();
    }

    setAllBands(restored);
    setEnabled(shouldBeEnabled);
}

}  // namespace ihub::dsp
