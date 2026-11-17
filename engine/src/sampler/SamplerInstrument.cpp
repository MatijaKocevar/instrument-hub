#include "SamplerInstrument.h"

#include "instrument/SamplerInstrumentConstants.h"

namespace ihub::sampler {

using namespace instrument_detail;

SamplerInstrument::SamplerInstrument() {
    for (auto& mode : padModes)
        mode.store(static_cast<int>(PadMode::oneShot), std::memory_order_relaxed);

    for (auto& group : chokeGroups) group.store(noChokeGroup, std::memory_order_relaxed);
}

SamplerInstrument::~SamplerInstrument() = default;

void SamplerInstrument::setKit(LoadedKit* newKit) noexcept {
    stopAllVoicesNow();
    kit.store(newKit, std::memory_order_release);
}

const LoadedKit* SamplerInstrument::getKit() const noexcept {
    return kit.load(std::memory_order_acquire);
}

void SamplerInstrument::setPadMode(int note, PadMode mode) noexcept {
    const auto index = (std::size_t)clampNote(note);
    padModes[index].store(static_cast<int>(mode), std::memory_order_relaxed);
}

SamplerInstrument::PadMode SamplerInstrument::getPadMode(int note) const noexcept {
    const auto value = padModes[(std::size_t)clampNote(note)].load(std::memory_order_relaxed);
    return value == static_cast<int>(PadMode::gate) ? PadMode::gate : PadMode::oneShot;
}

void SamplerInstrument::setChokeGroup(int note, int group) noexcept {
    chokeGroups[(std::size_t)clampNote(note)].store(clampGroup(group), std::memory_order_relaxed);
}

int SamplerInstrument::getChokeGroup(int note) const noexcept {
    return chokeGroups[(std::size_t)clampNote(note)].load(std::memory_order_relaxed);
}

void SamplerInstrument::setChokeFadeMilliseconds(float milliseconds) noexcept {
    chokeFadeMilliseconds.store(
        juce::jlimit(minimumChokeFadeMilliseconds, maximumChokeFadeMilliseconds, milliseconds),
        std::memory_order_relaxed);
}

float SamplerInstrument::getChokeFadeMilliseconds() const noexcept {
    return chokeFadeMilliseconds.load(std::memory_order_relaxed);
}

void SamplerInstrument::setGateReleaseMilliseconds(float milliseconds) noexcept {
    gateReleaseMilliseconds.store(
        juce::jlimit(minimumGateReleaseMilliseconds, maximumGateReleaseMilliseconds, milliseconds),
        std::memory_order_relaxed);
}

float SamplerInstrument::getGateReleaseMilliseconds() const noexcept {
    return gateReleaseMilliseconds.load(std::memory_order_relaxed);
}

void SamplerInstrument::reset() noexcept {
    stopAllVoicesNow();

    pendingReadIndex.store(pendingWriteIndex.load(std::memory_order_acquire),
                           std::memory_order_release);
}

void SamplerInstrument::prepare(double newSampleRate, int maxBlock) {
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax(1, maxBlock);
}

int SamplerInstrument::clampNote(int note) noexcept {
    return juce::jlimit(midiNoteMinimum, midiNoteMaximum, note);
}

int SamplerInstrument::clampGroup(int group) noexcept {
    return juce::jlimit(noChokeGroup, maximumChokeGroup, group);
}

int SamplerInstrument::getActiveVoiceCount() const noexcept {
    return activeVoiceCount.load(std::memory_order_relaxed);
}

int SamplerInstrument::getStolenVoiceCount() const noexcept {
    return stolenVoiceCount.load(std::memory_order_relaxed);
}

int SamplerInstrument::getDroppedEventCount() const noexcept {
    return droppedEventCount.load(std::memory_order_relaxed);
}

int SamplerInstrument::getStartedVoiceCount() const noexcept {
    return startedVoiceCount.load(std::memory_order_relaxed);
}

}  // namespace ihub::sampler
