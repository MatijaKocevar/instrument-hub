#include "SlotSwitcher.h"

#include <algorithm>
#include <cmath>

namespace AudioEngine {

namespace {
constexpr float halfPi = 1.57079632679489661923f;

float clampUnit(float value) noexcept {
  return juce::jlimit(0.0f, 1.0f, value);
}
}  // namespace

const char* SlotSwitcher::toString(State state) noexcept {
  switch (state) {
    case State::idle:
      return "idle";
    case State::warming:
      return "warming";
    case State::active:
      return "active";
    case State::fading:
      return "fading";
  }

  return "idle";
}

SlotSwitcher::SlotSwitcher(float fadeMs) noexcept
    : fadeMilliseconds(juce::jmax(0.0f, fadeMs)) {}

void SlotSwitcher::setSlots(const std::vector<Slot*>& newSlots) {
  slots = newSlots;
  numSlots = static_cast<int>(slots.size());

  runtime.assign(static_cast<size_t>(numSlots), RuntimeState{});

  const size_t publishedSize = static_cast<size_t>(juce::jmax(1, numSlots));
  publishedStates = std::make_unique<std::atomic<int>[]>(publishedSize);
  publishedGains = std::make_unique<std::atomic<float>[]>(publishedSize);

  for (int i = 0; i < numSlots; ++i) {
    publishedStates[static_cast<size_t>(i)].store(static_cast<int>(State::idle),
                                                  std::memory_order_relaxed);
    publishedGains[static_cast<size_t>(i)].store(0.0f,
                                                 std::memory_order_relaxed);
  }

  consumedSlot = -1;
  incomingSlot = -1;
  fadePosition = 0.0f;
  crossfading = false;

  if (prepared) prepare(sampleRate, maximumBlockSize, numChannels);
}

void SlotSwitcher::prepare(double newSampleRate, int newMaximumBlockSize,
                           int newNumChannels) {
  jassert(newSampleRate > 0.0);
  jassert(newMaximumBlockSize > 0);
  jassert(newNumChannels > 0);

  sampleRate = juce::jmax(1.0, newSampleRate);
  maximumBlockSize = juce::jmax(1, newMaximumBlockSize);
  numChannels = juce::jmax(1, newNumChannels);

  for (auto* slot : slots)
    if (slot != nullptr)
      slot->prepare(sampleRate, maximumBlockSize, numChannels);

  mixBuffer.setSize(numChannels, maximumBlockSize, false, true, true);
  mixBuffer.clear();

  const float fadeSamples =
      fadeMilliseconds * 0.001f * static_cast<float>(sampleRate);

  if (fadeMilliseconds <= 0.0f || fadeSamples < 1.0f)
    fadeIncrement = 1.0f;
  else
    fadeIncrement = 1.0f / fadeSamples;

  for (auto& state : runtime) state = RuntimeState{};

  consumedSlot = -1;
  incomingSlot = -1;
  fadePosition = 0.0f;
  crossfading = false;

  prepared = true;
  publishStates();
}

void SlotSwitcher::reset() noexcept {
  for (auto& state : runtime) state = RuntimeState{};

  pendingSlot.store(-1, std::memory_order_release);
  consumedSlot = -1;
  incomingSlot = -1;
  fadePosition = 0.0f;
  crossfading = false;

  mixBuffer.clear();
  publishStates();
}

Slot* SlotSwitcher::getSlot(int slotIndex) const noexcept {
  if (slotIndex < 0 || slotIndex >= numSlots) return nullptr;

  return slots[static_cast<size_t>(slotIndex)];
}

void SlotSwitcher::armSlot(int slotIndex) noexcept {
  if (slotIndex < 0) {
    disarm();
    return;
  }

  jassert(slotIndex < numSlots);

  if (slotIndex >= numSlots) return;

  pendingSlot.store(slotIndex, std::memory_order_release);
}

void SlotSwitcher::disarm() noexcept {
  pendingSlot.store(-1, std::memory_order_release);
}

int SlotSwitcher::getArmedSlot() const noexcept {
  return pendingSlot.load(std::memory_order_acquire);
}

SlotSwitcher::State SlotSwitcher::getSlotState(int slotIndex) const noexcept {
  if (slotIndex < 0 || slotIndex >= numSlots || publishedStates == nullptr)
    return State::idle;

  return static_cast<State>(
      publishedStates[static_cast<size_t>(slotIndex)].load(
          std::memory_order_relaxed));
}

float SlotSwitcher::getSlotGain(int slotIndex) const noexcept {
  if (slotIndex < 0 || slotIndex >= numSlots || publishedGains == nullptr)
    return 0.0f;

  return publishedGains[static_cast<size_t>(slotIndex)].load(
      std::memory_order_relaxed);
}

bool SlotSwitcher::process(int numSamples, juce::MidiBuffer& midi) noexcept {
  if (!prepared || numSlots <= 0 || numSamples <= 0) return false;

  const int samples = juce::jmin(numSamples, maximumBlockSize);

  if (samples <= 0) return false;

  const int requested = juce::jlimit(
      -1, numSlots - 1, pendingSlot.load(std::memory_order_acquire));

  if (requested != consumedSlot) {
    consumedSlot = requested;
    beginTransition(requested);
  }

  for (int channel = 0; channel < mixBuffer.getNumChannels(); ++channel)
    mixBuffer.clear(channel, 0, samples);

  bool renderedAny = false;

  for (int index = 0; index < numSlots; ++index) {
    auto& state = runtime[static_cast<size_t>(index)];

    if (state.state == State::idle) continue;

    Slot* slot = slots[static_cast<size_t>(index)];

    if (slot == nullptr || !slot->processUnfaded(samples, midi)) {
      state = RuntimeState{};
      continue;
    }

    renderedAny = true;

    const auto& source = slot->getOutputBuffer();
    const int sourceChannels = juce::jmin(numChannels, source.getNumChannels(),
                                          mixBuffer.getNumChannels());

    for (int frame = 0; frame < samples; ++frame) {
      const float gain =
          crossfading ? gainAt(state, fadePosition + static_cast<float>(frame) *
                                                         fadeIncrement)
                      : state.targetGain;

      for (int channel = 0; channel < sourceChannels; ++channel)
        mixBuffer.getWritePointer(channel)[frame] +=
            source.getReadPointer(channel)[frame] * gain;
    }
  }

  if (crossfading) {
    const float lastPhase =
        fadePosition + static_cast<float>(samples - 1) * fadeIncrement;

    for (auto& state : runtime)
      if (state.state != State::idle) state.gain = gainAt(state, lastPhase);

    fadePosition += static_cast<float>(samples) * fadeIncrement;

    if (fadePosition >= 1.0f) {
      for (auto& state : runtime) {
        state.gain = state.targetGain;

        if (state.state == State::warming)
          state.state = State::active;
        else if (state.state == State::fading)
          state.state = State::idle;
      }

      fadePosition = 1.0f;
      crossfading = false;
    }
  }

  publishStates();
  return renderedAny;
}

void SlotSwitcher::beginTransition(int slotIndex) noexcept {
  incomingSlot = (slotIndex >= 0 && slotIndex < numSlots) ? slotIndex : -1;
  fadePosition = 0.0f;

  bool needsFade = false;

  for (int index = 0; index < numSlots; ++index) {
    auto& state = runtime[static_cast<size_t>(index)];

    state.startGain = clampUnit(state.gain);

    if (index == incomingSlot) {
      state.targetGain = 1.0f;
      state.state = state.startGain >= 1.0f ? State::active : State::warming;
    } else {
      state.targetGain = 0.0f;
      state.state = state.startGain <= 0.0f ? State::idle : State::fading;
    }

    if (state.state == State::warming || state.state == State::fading)
      needsFade = true;
  }

  if (!needsFade || fadeIncrement >= 1.0f) {
    for (auto& state : runtime) {
      state.gain = state.targetGain;
      state.state = state.targetGain > 0.0f ? State::active : State::idle;
    }

    crossfading = false;
    fadePosition = 1.0f;
    publishStates();
    return;
  }

  crossfading = true;
  publishStates();
}

float SlotSwitcher::gainAt(const RuntimeState& state,
                           float phase) const noexcept {
  const float p = clampUnit(phase);
  const float sine = std::sin(p * halfPi);

  if (state.targetGain > 0.0f) {
    return state.startGain + (1.0f - state.startGain) * sine;
  }

  return state.startGain * std::cos(p * halfPi);
}

void SlotSwitcher::publishStates() noexcept {
  if (publishedStates == nullptr || publishedGains == nullptr) return;

  for (int index = 0; index < numSlots; ++index) {
    const auto& state = runtime[static_cast<size_t>(index)];

    publishedStates[static_cast<size_t>(index)].store(
        static_cast<int>(state.state), std::memory_order_relaxed);
    publishedGains[static_cast<size_t>(index)].store(state.gain,
                                                     std::memory_order_relaxed);
  }
}

}  // namespace AudioEngine
