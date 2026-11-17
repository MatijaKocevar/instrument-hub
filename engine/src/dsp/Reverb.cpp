#include "Reverb.h"

#include <algorithm>
#include <cmath>

namespace ihub::dsp {

namespace {
constexpr int maxSnapshotReadAttempts = 3;

constexpr int referenceSampleRate = 44100;
constexpr int combTuning[Reverb::numCombs]{1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
constexpr int allpassTuning[Reverb::numAllpasses]{556, 441, 341, 225};
constexpr int stereoSpread = 23;

constexpr float inputGain = 0.015f;

constexpr float allpassFeedback = 0.5f;

constexpr float dampingScale = 0.4f;

int scaledLength(int length, double sampleRate, int extra = 0) noexcept {
    const auto scale = sampleRate / static_cast<double>(referenceSampleRate);
    return juce::jmax(1, static_cast<int>(std::llround(length * scale)) + extra);
}
}  // namespace

void Reverb::Comb::prepare(int size) {
    buffer.assign(static_cast<std::size_t>(juce::jmax(1, size)), 0.0f);
    reset();
}

void Reverb::Comb::reset() noexcept {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    index = 0;
    filterStore = 0.0f;
}

float Reverb::Comb::process(float input, float feedback, float damp1, float damp2) noexcept {
    const auto output = buffer[static_cast<std::size_t>(index)];

    filterStore = output * damp2 + filterStore * damp1;
    buffer[static_cast<std::size_t>(index)] = input + filterStore * feedback;

    if (++index >= static_cast<int>(buffer.size())) index = 0;

    return output;
}

void Reverb::Allpass::prepare(int size) {
    buffer.assign(static_cast<std::size_t>(juce::jmax(1, size)), 0.0f);
    reset();
}

void Reverb::Allpass::reset() noexcept {
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    index = 0;
}

float Reverb::Allpass::process(float input, float feedback) noexcept {
    const auto buffered = buffer[static_cast<std::size_t>(index)];
    const auto output = -input + buffered;

    buffer[static_cast<std::size_t>(index)] = input + buffered * feedback;

    if (++index >= static_cast<int>(buffer.size())) index = 0;

    return output;
}

void Reverb::Channel::prepare(int sampleRate, int preDelaySize, bool useStereoSpread) {
    const auto rate = sampleRate > 0 ? static_cast<double>(sampleRate) : 48000.0;

    for (int i = 0; i < Reverb::numCombs; ++i)
        combs[static_cast<std::size_t>(i)].prepare(
            scaledLength(combTuning[i], rate, useStereoSpread ? stereoSpread : 0));

    for (int i = 0; i < Reverb::numAllpasses; ++i)
        allpasses[static_cast<std::size_t>(i)].prepare(scaledLength(allpassTuning[i], rate));

    preDelayBuffer.assign(static_cast<std::size_t>(juce::jmax(1, preDelaySize)), 0.0f);
    preDelayIndex = 0;
}

void Reverb::Channel::reset() noexcept {
    for (auto& comb : combs) comb.reset();

    for (auto& allpass : allpasses) allpass.reset();

    std::fill(preDelayBuffer.begin(), preDelayBuffer.end(), 0.0f);
    preDelayIndex = 0;
}

float Reverb::Channel::process(float dry, int preDelaySamples, float feedback, float damp1,
                               float damp2) noexcept {
    float input = dry;

    if (!preDelayBuffer.empty()) {
        const int size = static_cast<int>(preDelayBuffer.size());
        const int preDelay = juce::jlimit(0, size - 1, preDelaySamples);

        if (preDelay > 0) {
            int read = preDelayIndex - preDelay;

            if (read < 0) read += size;

            input = preDelayBuffer[static_cast<std::size_t>(read)];
        }

        preDelayBuffer[static_cast<std::size_t>(preDelayIndex)] = dry;

        if (++preDelayIndex >= size) preDelayIndex = 0;
    }

    input *= inputGain;

    float sum = 0.0f;

    for (auto& comb : combs) sum += comb.process(input, feedback, damp1, damp2);

    for (auto& allpass : allpasses) sum = allpass.process(sum, allpassFeedback);

    return sum;
}

Reverb::Reverb()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    applied = computePublished();
    publishSettings();
    appliedVersion = snapshot.version.load(std::memory_order_acquire);
}

void Reverb::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;

    const auto numChannels = juce::jmax(1, juce::jmin(2, getTotalNumInputChannels()));
    const auto rate = static_cast<int>(std::lround(currentSampleRate));

    maxPreDelaySamples = static_cast<int>(std::ceil(maxPreDelayMs * 0.001 * currentSampleRate)) + 1;

    channels.clear();
    channels.resize(static_cast<std::size_t>(numChannels));

    for (int channel = 0; channel < numChannels; ++channel)
        channels[static_cast<std::size_t>(channel)].prepare(rate, maxPreDelaySamples, channel == 1);

    reset();

    applied = computePublished();
    publishSettings();
    appliedVersion = snapshot.version.load(std::memory_order_acquire);

    prepared = true;

    setLatencySamples(0);
}

void Reverb::releaseResources() { prepared = false; }

void Reverb::reset() {
    for (auto& channel : channels) channel.reset();
}

void Reverb::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    const juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0) return;

    if (!prepared) {
        buffer.clear();
        return;
    }

    const auto version = snapshot.version.load(std::memory_order_acquire);

    if (version != appliedVersion && (version & 1u) == 0u) {
        Published pending;

        if (readSnapshot(pending)) {
            applied = pending;
            appliedVersion = version;
        }
    }

    const bool bypassed = !enabled.load(std::memory_order_relaxed);

    auto* const output = buffer.getArrayOfWritePointers();

    const int channelsToProcess = juce::jmin(numChannels, static_cast<int>(channels.size()));

    const bool stereo = channelsToProcess >= 2;

    const float feedback = applied.combFeedback;
    const float damp1 = applied.damp1;
    const float damp2 = applied.damp2;
    const float wetA = applied.wet1;
    const float wetB = applied.wet2;
    const float dryGain = applied.dryGain;
    const int preDelaySamples = juce::jlimit(0, maxPreDelaySamples, applied.preDelaySamples);

    auto& left = channels[0];

    for (int i = 0; i < numSamples; ++i) {
        const float dryLeft = output[0][i];
        const float wetLeft = left.process(dryLeft, preDelaySamples, feedback, damp1, damp2);

        if (stereo) {
            auto& right = channels[1];

            const float dryRight = output[1][i];
            const float wetRight = right.process(dryRight, preDelaySamples, feedback, damp1, damp2);

            if (!bypassed) {
                output[0][i] = dryLeft * dryGain + wetLeft * wetA + wetRight * wetB;
                output[1][i] = dryRight * dryGain + wetRight * wetA + wetLeft * wetB;
            }
        } else if (!bypassed) {
            output[0][i] = dryLeft * dryGain + wetLeft * (wetA + wetB);
        }
    }

    for (int channel = channelsToProcess; channel < numChannels; ++channel)
        buffer.clear(channel, 0, numSamples);
}

bool Reverb::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto inputs = layouts.getMainInputChannelSet();
    const auto outputs = layouts.getMainOutputChannelSet();

    if (inputs != outputs) return false;

    return inputs == juce::AudioChannelSet::mono() || inputs == juce::AudioChannelSet::stereo();
}

ReverbSettings Reverb::clampSettings(const ReverbSettings& input) noexcept {
    ReverbSettings s = input;

    s.roomSize = juce::jlimit(minRoomSize, maxRoomSize, s.roomSize);
    s.damping = juce::jlimit(minDamping, maxDamping, s.damping);
    s.width = juce::jlimit(minWidth, maxWidth, s.width);
    s.mix = juce::jlimit(0.0f, 1.0f, s.mix);
    s.preDelayMs = juce::jlimit(minPreDelayMs, maxPreDelayMs, s.preDelayMs);

    return s;
}

Reverb::Published Reverb::computePublished() const noexcept {
    const auto clamped = clampSettings(settings);

    Published p;

    p.combFeedback = combFeedbackOffset + combFeedbackScale * clamped.roomSize;
    p.damp1 = clamped.damping * dampingScale;
    p.damp2 = 1.0f - p.damp1;

    p.wet1 = clamped.mix * (clamped.width * 0.5f + 0.5f);
    p.wet2 = clamped.mix * ((1.0f - clamped.width) * 0.5f);
    p.dryGain = 1.0f - clamped.mix;

    p.preDelaySamples = static_cast<int>(
        std::llround(static_cast<double>(clamped.preDelayMs) * 0.001 * currentSampleRate));

    return p;
}

void Reverb::setSettings(const ReverbSettings& newSettings) {
    settings = clampSettings(newSettings);
    publishSettings();
}

ReverbSettings Reverb::getSettings() const noexcept { return settings; }

void Reverb::setRoomSize(float roomSize) {
    auto s = settings;
    s.roomSize = roomSize;
    setSettings(s);
}

void Reverb::setDamping(float damping) {
    auto s = settings;
    s.damping = damping;
    setSettings(s);
}

void Reverb::setWidth(float width) {
    auto s = settings;
    s.width = width;
    setSettings(s);
}

void Reverb::setMix(float mix) {
    auto s = settings;
    s.mix = mix;
    setSettings(s);
}

void Reverb::setPreDelayMs(float milliseconds) {
    auto s = settings;
    s.preDelayMs = milliseconds;
    setSettings(s);
}

void Reverb::setEnabled(bool shouldBeEnabled) {
    enabled.store(shouldBeEnabled, std::memory_order_relaxed);
}

bool Reverb::isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }

void Reverb::publishSettings() noexcept {
    const auto publishedValues = computePublished();

    snapshot.version.fetch_add(1, std::memory_order_acq_rel);
    snapshot.values.store(publishedValues);
    snapshot.version.fetch_add(1, std::memory_order_release);
}

bool Reverb::readSnapshot(Published& destination) const noexcept {
    for (int attempt = 0; attempt < maxSnapshotReadAttempts; ++attempt) {
        const auto before = snapshot.version.load(std::memory_order_acquire);

        if ((before & 1u) != 0u) continue;

        destination = snapshot.values.load();

        std::atomic_thread_fence(std::memory_order_acquire);

        const auto after = snapshot.version.load(std::memory_order_acquire);

        if (before == after) return true;
    }

    return false;
}

void Reverb::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream stream(destData, false);

    stream.writeInt(1);
    stream.writeBool(isEnabled());
    stream.writeFloat(settings.roomSize);
    stream.writeFloat(settings.damping);
    stream.writeFloat(settings.width);
    stream.writeFloat(settings.mix);
    stream.writeFloat(settings.preDelayMs);
}

void Reverb::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) return;

    juce::MemoryInputStream stream(data, static_cast<std::size_t>(sizeInBytes), false);

    if (stream.readInt() != 1) return;

    constexpr int expectedBytes = 4 + 1 + 5 * 4;

    if (sizeInBytes < expectedBytes) return;

    const auto shouldBeEnabled = stream.readBool();

    ReverbSettings restored;
    restored.roomSize = stream.readFloat();
    restored.damping = stream.readFloat();
    restored.width = stream.readFloat();
    restored.mix = stream.readFloat();
    restored.preDelayMs = stream.readFloat();

    setSettings(restored);
    setEnabled(shouldBeEnabled);
}

}  // namespace ihub::dsp
