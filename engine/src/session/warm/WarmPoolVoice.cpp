#include "session/warm/WarmPoolVoice.h"

#include "session/WarmPoolController.h"

namespace ihub::session {

void WarmPoolController::bindVoice(int slot, AudioEngine::SlotSource* instance) noexcept {
    if (slot < 0 || slot >= static_cast<int>(voices.size())) return;

    auto* voice =
        dynamic_cast<detail::WarmPoolVoice*>(voices[static_cast<std::size_t>(slot)].get());

    if (voice != nullptr) voice->bind(instance);
}

void WarmPoolController::clearVoice(AudioEngine::SlotSource* instance) noexcept {
    if (instance == nullptr) return;

    for (auto& voiceSource : voices)
        if (auto* voice = dynamic_cast<detail::WarmPoolVoice*>(voiceSource.get()))
            voice->clearIf(instance);
}

namespace detail {

void WarmPoolVoice::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    auto* instance = delegate.load(std::memory_order_acquire);

    if (instance == nullptr) {
        buffer.clear();
        return;
    }

    instance->process(buffer, midi);
}

void WarmPoolVoice::setInputBlock(const float* const* channelData, int numInputs) noexcept {
    auto* instance = delegate.load(std::memory_order_acquire);

    if (instance != nullptr && acceptsInput.load(std::memory_order_relaxed))
        instance->setInputBlock(channelData, numInputs);
}

void WarmPoolVoice::bind(AudioEngine::SlotSource* instance) noexcept {
    int latencySamples = 0;
    bool accepts = false;
    bool bypasses = false;

    if (instance != nullptr) {
        latencySamples = instance->latencySamples();
        accepts = instance->acceptsDeviceInput();
        bypasses = instance->bypassesFxChain();
    }

    latency.store(latencySamples, std::memory_order_relaxed);
    acceptsInput.store(accepts, std::memory_order_relaxed);
    bypassesChain.store(bypasses, std::memory_order_relaxed);
    delegate.store(instance, std::memory_order_release);
}

void WarmPoolVoice::clearIf(AudioEngine::SlotSource* instance) noexcept {
    if (delegate.load(std::memory_order_acquire) != instance) return;

    delegate.store(nullptr, std::memory_order_release);
    latency.store(0, std::memory_order_relaxed);
    acceptsInput.store(false, std::memory_order_relaxed);
    bypassesChain.store(false, std::memory_order_relaxed);
}

}  // namespace detail
}  // namespace ihub::session
