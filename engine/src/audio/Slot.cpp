#include "Slot.h"

#include <algorithm>
#include <cmath>

namespace AudioEngine
{

namespace
{
constexpr float userGainRampMilliseconds = 10.0f;
constexpr float minimumFadeMilliseconds = 0.5f;
constexpr float maximumGain = 4.0f;
} // namespace

const char* toString (SlotState state) noexcept
{
    switch (state)
    {
        case SlotState::idle:      return "idle";
        case SlotState::fadingIn:  return "fading-in";
        case SlotState::active:    return "active";
        case SlotState::fadingOut: return "fading-out";
    }

    return "idle";
}

Slot::Slot (int indexToUse, float fadeMs)
    : index (indexToUse),
      fadeMilliseconds (juce::jmax (minimumFadeMilliseconds, fadeMs))
{
}

void Slot::prepare (double newSampleRate, int newMaximumBlockSize, int newNumChannels)
{
    jassert (newSampleRate > 0.0);
    jassert (newMaximumBlockSize > 0);
    jassert (newNumChannels > 0);

    sampleRate = newSampleRate;
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    numChannels = juce::jmax (1, newNumChannels);

    storage.setSize (numChannels, maximumBlockSize, false, true, true);
    storage.clear();

    // `view` refers to `storage` for every block. Rebinding it keeps the
    // per-block channel pointers in place without growing a heap allocation:
    // the channel count never increases inside process().
    view.setSize (0, 0, false, true, true);
    view.setDataToReferTo (storage.getArrayOfWritePointers(), numChannels, 0);

    const float fadeSamples = juce::jmax (1.0f,
                                          fadeMilliseconds * 0.001f * static_cast<float> (sampleRate));
    fadeStep = 1.0f / fadeSamples;

    const float gainSamples = juce::jmax (1.0f,
                                          userGainRampMilliseconds * 0.001f
                                              * static_cast<float> (sampleRate));
    gainStep = 1.0f / gainSamples;

    state = SlotState::idle;
    fadeGain = 0.0f;
    appliedGain = juce::jlimit (0.0f, maximumGain,
                                gainTarget.load (std::memory_order_relaxed));
    publishedState.store (SlotState::idle, std::memory_order_relaxed);

    prepared = true;

    // Keep the activation request across a device change; it fades back in on
    // the first block rather than silently going quiet.
    if (source != nullptr)
        source->prepare (sampleRate, maximumBlockSize);

    fxChain.prepare (sampleRate, maximumBlockSize, numChannels);
}

void Slot::reset() noexcept
{
    state = SlotState::idle;
    fadeGain = 0.0f;
    appliedGain = juce::jlimit (0.0f, maximumGain,
                                gainTarget.load (std::memory_order_relaxed));
    activated.store (false, std::memory_order_relaxed);
    publishedState.store (SlotState::idle, std::memory_order_relaxed);
    storage.clear();
    fxChain.reset();
}

void Slot::setSource (SlotSource* newSource) noexcept
{
    source = newSource;

    if (source != nullptr && prepared)
        source->prepare (sampleRate, maximumBlockSize);
}

void Slot::activate() noexcept
{
    activated.store (true, std::memory_order_release);
}

void Slot::deactivate() noexcept
{
    activated.store (false, std::memory_order_release);
}

bool Slot::isActivated() const noexcept
{
    return activated.load (std::memory_order_acquire);
}

void Slot::setGain (float newGain) noexcept
{
    gainTarget.store (juce::jlimit (0.0f, maximumGain, newGain),
                      std::memory_order_relaxed);
}

float Slot::getGain() const noexcept
{
    return gainTarget.load (std::memory_order_relaxed);
}

SlotState Slot::getState() const noexcept
{
    return publishedState.load (std::memory_order_relaxed);
}

bool Slot::isProcessing() const noexcept
{
    return getState() != SlotState::idle;
}

bool Slot::process (int numSamples, juce::MidiBuffer& midi) noexcept
{
    if (! prepared || numSamples <= 0)
        return false;

    const int samples = juce::jmin (numSamples, maximumBlockSize);
    const bool wantActive = activated.load (std::memory_order_acquire);

    switch (state)
    {
        case SlotState::idle:
            if (! wantActive)
                return false;

            state = SlotState::fadingIn;
            break;

        case SlotState::active:
        case SlotState::fadingIn:
            if (! wantActive)
                state = SlotState::fadingOut;
            break;

        case SlotState::fadingOut:
            if (wantActive)
                state = SlotState::fadingIn;
            break;
    }

    if (source == nullptr)
    {
        // Nothing to render; do not stay in a "processing" state forever.
        state = SlotState::idle;
        fadeGain = 0.0f;
        publishedState.store (state, std::memory_order_relaxed);
        return false;
    }

    if (! renderSource (samples, midi))
        return false;

    applyGains (samples, true);

    publishedState.store (state, std::memory_order_relaxed);
    return true;
}

bool Slot::processUnfaded (int numSamples, juce::MidiBuffer& midi) noexcept
{
    if (! prepared || numSamples <= 0 || source == nullptr)
        return false;

    const int samples = juce::jmin (numSamples, maximumBlockSize);

    if (samples <= 0 || ! renderSource (samples, midi))
        return false;

    applyGains (samples, false);
    return true;
}

void Slot::feedDeviceInput (const float* const* channelData, int newNumChannels) noexcept
{
    if (source != nullptr && source->acceptsDeviceInput())
        source->setInputBlock (channelData, newNumChannels);
}

bool Slot::renderSource (int numSamples, juce::MidiBuffer& midi) noexcept
{
    view.setDataToReferTo (storage.getArrayOfWritePointers(), numChannels, numSamples);
    view.clear();
    source->process (view, midi);

    // instrument -> ordered FX chain -> bus. An empty chain returns without
    // touching the buffer, so this stays a bit-exact passthrough.
    //
    // Direct monitoring asks the slot to leave the chain out of the audible
    // path: the input reaches the bus at the source's own latency instead of
    // waiting for a heavy effect.
    if (! source->bypassesFxChain())
        fxChain.process (view, midi);

    return true;
}

void Slot::applyGains (int numSamples, bool applyFade) noexcept
{
    auto* const* channels = view.getArrayOfWritePointers();
    const float target = juce::jlimit (0.0f, maximumGain,
                                      gainTarget.load (std::memory_order_relaxed));

    for (int i = 0; i < numSamples; ++i)
    {
        if (applyFade && state == SlotState::fadingIn)
        {
            fadeGain = juce::jmin (1.0f, fadeGain + fadeStep);

            if (fadeGain >= 1.0f)
            {
                fadeGain = 1.0f;
                state = SlotState::active;
            }
        }
        else if (applyFade && state == SlotState::fadingOut)
        {
            fadeGain = juce::jmax (0.0f, fadeGain - fadeStep);

            if (fadeGain <= 0.0f)
            {
                fadeGain = 0.0f;
                state = SlotState::idle;
            }
        }

        if (std::abs (target - appliedGain) > 0.0f)
        {
            appliedGain += juce::jlimit (-gainStep, gainStep, target - appliedGain);

            if (std::abs (target - appliedGain) <= gainStep * 0.5f)
                appliedGain = target;
        }

        const float gain = (applyFade ? fadeGain : 1.0f) * appliedGain;

        // Unity is the overwhelmingly common case; skipping the multiply also
        // keeps bit-exact unity gain.
        if (std::abs (gain - 1.0f) <= 0.0f)
            continue;

        for (int channel = 0; channel < numChannels; ++channel)
            channels[channel][i] *= gain;
    }
}

int Slot::latencySamples() const noexcept
{
    const int sourceLatency = source != nullptr ? source->latencySamples() : 0;

    // A bypassed chain adds no delay to the audible path, so plugin delay
    // compensation must not over-delay the other slots for it.
    if (source != nullptr && source->bypassesFxChain())
        return sourceLatency;

    return sourceLatency + fxChain.latencySamples();
}

double Slot::latencyMilliseconds() const noexcept
{
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double> (latencySamples()) * 1000.0 / sampleRate;
}

} // namespace AudioEngine
