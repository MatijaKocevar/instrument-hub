#include "audio/FxRack.h"

#include "dsp/Delay.h"

namespace AudioEngine {

FxRack::FxRack(juce::AudioPluginFormatManager* formatManagerToUse)
    : formatManager(formatManagerToUse) {}

void FxRack::rebuild(FxChain& chain, double sampleRate, int maximumBlockSize, int numChannels) {
    juce::ignoreUnused(sampleRate, maximumBlockSize, numChannels);

    chain.clear();

    for (auto& insert : inserts)
        if (insert.effect != nullptr) chain.add(insert.effect.get());

    chain.refreshLatency();
}

void FxRack::setTransport(const FxTransport& newTransport) noexcept {
    transport = newTransport;

    for (auto& insert : inserts)
        if (insert.effect != nullptr) applyTransportTo(*insert.effect);
}

void FxRack::applyTransportTo(InsertEffect& effect) const {
    if (effect.getType() != FxInsertType::delay) return;

    if (auto* delay = static_cast<ihub::dsp::Delay*>(effect.getWrappedProcessor())) {
        ihub::dsp::DelayTransport delayTransport;
        delayTransport.playing = transport.playing;
        delayTransport.bpm = transport.bpm;
        delayTransport.ppq = transport.ppq;
        delay->setTransport(delayTransport);
    }
}

}  // namespace AudioEngine
