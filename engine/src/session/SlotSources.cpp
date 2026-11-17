#include "SlotSources.h"

#include "instruments/Instrument.h"
#include "sampler/KitLoader.h"
#include "sampler/SamplerInstrument.h"

#include <cmath>
#include <memory>

namespace ihub::session {

namespace {
class InitToneSource final : public Instrument, public AudioEngine::SlotSource {
public:
    InitToneSource(double frequencyToUse, float amplitudeToUse)
        : frequency(frequencyToUse > 0.0 ? frequencyToUse : 220.0), amplitude(amplitudeToUse) {}

    void prepare(double newSampleRate, int) override {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        phase = 0.0;
        increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    }

    void noteOn(int ch, int note, float vel) override { juce::ignoreUnused(ch, note, vel); }

    void noteOff(int ch, int note) override { juce::ignoreUnused(ch, note); }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        const int samples = buffer.getNumSamples();
        const int channels = buffer.getNumChannels();

        for (int frame = 0; frame < samples; ++frame) {
            const auto value = static_cast<float>(amplitude * std::sin(phase));

            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer(channel)[frame] = value;

            phase += increment;

            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
    }

    int latencySamples() const noexcept override { return 0; }

    Kind kind() const noexcept override { return Kind::Sampler; }

private:
    const double frequency;
    const float amplitude;

    double sampleRate = 48000.0;
    double phase = 0.0;
    double increment = 0.0;
};

class SilentSource final : public AudioEngine::SlotSource {
public:
    void prepare(double, int) override {}

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }

    int latencySamples() const noexcept override { return 0; }
};

class PooledSamplerSource final : public AudioEngine::SlotSource {
public:
    bool load(const juce::File& file, double sampleRate, juce::String& error) {
        kit = std::make_unique<sampler::LoadedKit>();

        if (!sampler::KitLoader::loadFromFile(file, sampleRate, *kit, error)) {
            kit.reset();
            return false;
        }

        instrument = std::make_unique<sampler::SamplerInstrument>();
        instrument->setKit(kit.get());
        return true;
    }

    void prepare(double sampleRate, int maximumBlockSize) override {
        if (instrument != nullptr) instrument->prepare(sampleRate, maximumBlockSize);
    }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        if (instrument != nullptr)
            instrument->process(buffer, midi);
        else
            buffer.clear();
    }

    int latencySamples() const noexcept override {
        return instrument != nullptr ? instrument->latencySamples() : 0;
    }

private:
    std::unique_ptr<sampler::LoadedKit> kit;
    std::unique_ptr<sampler::SamplerInstrument> instrument;
};

}  // namespace

std::unique_ptr<AudioEngine::SlotSource> makeInitToneSource(double frequency, float amplitude) {
    return std::make_unique<InitToneSource>(frequency, amplitude);
}

std::unique_ptr<AudioEngine::SlotSource> makeSilentSource() {
    return std::make_unique<SilentSource>();
}

std::unique_ptr<AudioEngine::SlotSource> makeSamplerSource(const juce::File& file,
                                                           double sampleRate, juce::String& error) {
    auto source = std::make_unique<PooledSamplerSource>();

    if (!source->load(file, sampleRate, error)) return nullptr;

    return source;
}

}  // namespace ihub::session
