#include "../SamplerInstrument.h"

#include "SamplerInstrumentConstants.h"

namespace ihub::sampler
{

using namespace instrument_detail;

float SamplerInstrument::fadeStepForMilliseconds (float milliseconds) const noexcept
{
    const auto samples = juce::jmax (1.0f,
                                     milliseconds * 0.001f * (float) sampleRate);
    return 1.0f / samples;
}

void SamplerInstrument::startVoice (int channel, int note, int velocity) noexcept
{
    auto* loaded = kit.load (std::memory_order_acquire);

    if (loaded == nullptr)
        return;

    const auto clampedNote = clampNote (note);
    const auto clampedVelocity = juce::jlimit (midiVelocityMinimum,
                                               midiVelocityMaximum,
                                               velocity);

    const auto hit = loaded->resolve (clampedNote, clampedVelocity);

    if (! hit.isValid())
        return;

    const auto group = chokeGroups[(std::size_t) clampedNote].load (std::memory_order_relaxed);

    if (group != noChokeGroup)
        chokeVoices (group, -1);

    int index = findFreeVoice();
    bool reusedActiveVoice = false;

    if (index < 0)
    {
        index = findVoiceToSteal();
        reusedActiveVoice = index >= 0;

        if (index >= 0)
            stolenVoiceCount.fetch_add (1, std::memory_order_relaxed);
    }

    if (index < 0)
        return;

    if (! reusedActiveVoice)
        activeVoiceCount.fetch_add (1, std::memory_order_relaxed);

    auto& voice = voices[(std::size_t) index];

    voice.sample = hit.sample;
    voice.note = clampedNote;
    voice.channel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);
    voice.chokeGroup = group;
    voice.position = 0.0;
    voice.ratio = hit.playbackRatio > 0.0 ? hit.playbackRatio : 1.0;
    voice.leftGain = hit.leftGain();
    voice.rightGain = hit.rightGain();
    voice.gain = 1.0f;
    voice.fadeStep = 0.0f;
    voice.serial = nextSerial++;
    voice.mode = getPadMode (clampedNote);
    voice.active = true;

    startedVoiceCount.fetch_add (1, std::memory_order_relaxed);
}

void SamplerInstrument::releaseVoices (int channel, int note) noexcept
{
    const auto clampedNote = clampNote (note);
    const auto step = fadeStepForMilliseconds (
        gateReleaseMilliseconds.load (std::memory_order_relaxed));

    for (auto& voice : voices)
    {
        if (! voice.active || voice.mode != PadMode::gate)
            continue;

        if (voice.note != clampedNote)
            continue;

        if (channel > 0 && voice.channel != channel)
            continue;

        voice.fadeStep = juce::jmax (voice.fadeStep, step);
    }
}

void SamplerInstrument::chokeVoices (int group, int excludeVoice) noexcept
{
    if (group == noChokeGroup)
        return;

    const auto step = fadeStepForMilliseconds (
        chokeFadeMilliseconds.load (std::memory_order_relaxed));

    for (int i = 0; i < maxVoices; ++i)
    {
        if (i == excludeVoice)
            continue;

        auto& voice = voices[(std::size_t) i];

        if (! voice.active || voice.chokeGroup != group)
            continue;

        voice.fadeStep = juce::jmax (voice.fadeStep, step);
    }
}

void SamplerInstrument::allNotesOffNow() noexcept
{
    const auto step = fadeStepForMilliseconds (
        gateReleaseMilliseconds.load (std::memory_order_relaxed));

    for (auto& voice : voices)
        if (voice.active && voice.mode == PadMode::gate)
            voice.fadeStep = juce::jmax (voice.fadeStep, step);
}

void SamplerInstrument::stopAllVoicesNow() noexcept
{
    for (auto& voice : voices)
    {
        voice = {};
        voice.active = false;
    }

    activeVoiceCount.store (0, std::memory_order_relaxed);
}

int SamplerInstrument::findFreeVoice() const noexcept
{
    for (int i = 0; i < maxVoices; ++i)
        if (! voices[(std::size_t) i].active)
            return i;

    return -1;
}

int SamplerInstrument::findVoiceToSteal() const noexcept
{
    int best = -1;
    std::uint64_t oldest = 0;

    for (int i = 0; i < maxVoices; ++i)
    {
        const auto& voice = voices[(std::size_t) i];

        if (! voice.active)
            continue;

        if (best < 0 || voice.serial < oldest)
        {
            best = i;
            oldest = voice.serial;
        }
    }

    return best;
}

void SamplerInstrument::setVoiceInactive (Voice& voice) noexcept
{
    if (voice.active)
    {
        voice.active = false;
        activeVoiceCount.fetch_sub (1, std::memory_order_relaxed);
    }
}

}
