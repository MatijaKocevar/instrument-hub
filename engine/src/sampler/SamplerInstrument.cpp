#include "SamplerInstrument.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ihub::sampler
{

namespace
{
constexpr int midiChannelMinimum = 1;
constexpr int midiChannelMaximum = 16;
constexpr int midiNoteMinimum = 0;
constexpr int midiNoteMaximum = 127;
constexpr int midiVelocityMinimum = 1;
constexpr int midiVelocityMaximum = 127;

constexpr float minimumGateReleaseMilliseconds = 0.1f;
constexpr float maximumGateReleaseMilliseconds = 100.0f;
constexpr float minimumChokeFadeMilliseconds = 0.1f;

constexpr int maximumMidiBytes = 3;

constexpr float monoPanSum = 0.70710678118f;
} // namespace

//==============================================================================
SamplerInstrument::SamplerInstrument()
{
    for (auto& mode : padModes)
        mode.store (static_cast<int> (PadMode::oneShot), std::memory_order_relaxed);

    for (auto& group : chokeGroups)
        group.store (noChokeGroup, std::memory_order_relaxed);
}

SamplerInstrument::~SamplerInstrument() = default;

//==============================================================================
void SamplerInstrument::setKit (LoadedKit* newKit) noexcept
{
    // Changing the kit behind a sounding voice would leave a dangling sample
    // pointer, so stop everything first. The caller keeps the stream stopped.
    stopAllVoicesNow();
    kit.store (newKit, std::memory_order_release);
}

const LoadedKit* SamplerInstrument::getKit() const noexcept
{
    return kit.load (std::memory_order_acquire);
}

void SamplerInstrument::setPadMode (int note, PadMode mode) noexcept
{
    const auto index = (std::size_t) clampNote (note);
    padModes[index].store (static_cast<int> (mode), std::memory_order_relaxed);
}

SamplerInstrument::PadMode SamplerInstrument::getPadMode (int note) const noexcept
{
    const auto value = padModes[(std::size_t) clampNote (note)].load (std::memory_order_relaxed);
    return value == static_cast<int> (PadMode::gate) ? PadMode::gate : PadMode::oneShot;
}

void SamplerInstrument::setChokeGroup (int note, int group) noexcept
{
    chokeGroups[(std::size_t) clampNote (note)].store (clampGroup (group),
                                                       std::memory_order_relaxed);
}

int SamplerInstrument::getChokeGroup (int note) const noexcept
{
    return chokeGroups[(std::size_t) clampNote (note)].load (std::memory_order_relaxed);
}

void SamplerInstrument::setChokeFadeMilliseconds (float milliseconds) noexcept
{
    chokeFadeMilliseconds.store (
        juce::jlimit (minimumChokeFadeMilliseconds, maximumChokeFadeMilliseconds, milliseconds),
        std::memory_order_relaxed);
}

float SamplerInstrument::getChokeFadeMilliseconds() const noexcept
{
    return chokeFadeMilliseconds.load (std::memory_order_relaxed);
}

void SamplerInstrument::setGateReleaseMilliseconds (float milliseconds) noexcept
{
    gateReleaseMilliseconds.store (
        juce::jlimit (minimumGateReleaseMilliseconds,
                      maximumGateReleaseMilliseconds,
                      milliseconds),
        std::memory_order_relaxed);
}

float SamplerInstrument::getGateReleaseMilliseconds() const noexcept
{
    return gateReleaseMilliseconds.load (std::memory_order_relaxed);
}

void SamplerInstrument::reset() noexcept
{
    stopAllVoicesNow();

    pendingReadIndex.store (pendingWriteIndex.load (std::memory_order_acquire),
                            std::memory_order_release);
}

//==============================================================================
void SamplerInstrument::prepare (double newSampleRate, int maxBlock)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (1, maxBlock);
}

int SamplerInstrument::clampNote (int note) noexcept
{
    return juce::jlimit (midiNoteMinimum, midiNoteMaximum, note);
}

int SamplerInstrument::clampGroup (int group) noexcept
{
    return juce::jlimit (noChokeGroup, maximumChokeGroup, group);
}

float SamplerInstrument::fadeStepForMilliseconds (float milliseconds) const noexcept
{
    const auto samples = juce::jmax (1.0f,
                                     milliseconds * 0.001f * (float) sampleRate);
    return 1.0f / samples;
}

//==============================================================================
void SamplerInstrument::noteOn (int channel, int note, float velocity)
{
    if (velocity <= 0.0f)
    {
        noteOff (channel, note);
        return;
    }

    const auto clampedChannel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);
    const auto clampedNote = clampNote (note);
    const auto midiVelocity = juce::jlimit (
        midiVelocityMinimum, midiVelocityMaximum,
        (int) std::lround (velocity * (float) midiVelocityMaximum));

    const std::uint8_t message[maximumMidiBytes] {
        static_cast<std::uint8_t> (0x90 | (clampedChannel - 1)),
        static_cast<std::uint8_t> (clampedNote),
        static_cast<std::uint8_t> (midiVelocity)
    };

    pushEvent (message, maximumMidiBytes);
}

void SamplerInstrument::noteOff (int channel, int note)
{
    const auto clampedChannel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);

    const std::uint8_t message[maximumMidiBytes] {
        static_cast<std::uint8_t> (0x80 | (clampedChannel - 1)),
        static_cast<std::uint8_t> (clampNote (note)),
        0
    };

    pushEvent (message, maximumMidiBytes);
}

void SamplerInstrument::allNotesOff (int channel)
{
    const auto clampedChannel = juce::jlimit (midiChannelMinimum, midiChannelMaximum, channel);

    const std::uint8_t message[maximumMidiBytes] {
        static_cast<std::uint8_t> (0xB0 | (clampedChannel - 1)),
        123,
        0
    };

    pushEvent (message, maximumMidiBytes);
}

bool SamplerInstrument::pushEvent (const std::uint8_t* bytes, int numBytes) noexcept
{
    const auto write = pendingWriteIndex.load (std::memory_order_relaxed);
    const auto next = (write + 1) % maxPendingEvents;

    if (next == pendingReadIndex.load (std::memory_order_acquire))
    {
        droppedEventCount.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    auto& event = pendingEvents[write];

    for (int i = 0; i < numBytes; ++i)
        event.bytes[i] = bytes[i];

    event.numBytes = static_cast<std::uint8_t> (juce::jlimit (0, maximumMidiBytes, numBytes));

    // Publishing the write index makes the event visible to process().
    pendingWriteIndex.store (next, std::memory_order_release);
    return true;
}

int SamplerInstrument::getPendingEventCount() const noexcept
{
    const auto read = pendingReadIndex.load (std::memory_order_acquire);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    return (int) ((write + maxPendingEvents - read) % maxPendingEvents);
}

int SamplerInstrument::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount.load (std::memory_order_relaxed);
}

int SamplerInstrument::getStolenVoiceCount() const noexcept
{
    return stolenVoiceCount.load (std::memory_order_relaxed);
}

int SamplerInstrument::getDroppedEventCount() const noexcept
{
    return droppedEventCount.load (std::memory_order_relaxed);
}

int SamplerInstrument::getStartedVoiceCount() const noexcept
{
    return startedVoiceCount.load (std::memory_order_relaxed);
}

//==============================================================================
void SamplerInstrument::drainQueuedEvents() noexcept
{
    auto read = pendingReadIndex.load (std::memory_order_relaxed);
    const auto write = pendingWriteIndex.load (std::memory_order_acquire);

    while (read != write)
    {
        applyQueuedEvent (pendingEvents[read]);
        read = (read + 1) % maxPendingEvents;
    }

    pendingReadIndex.store (read, std::memory_order_release);
}

void SamplerInstrument::applyQueuedEvent (const PendingEvent& event) noexcept
{
    if (event.numBytes < 2)
        return;

    const auto status = event.bytes[0];
    const auto type = status & 0xF0;
    const auto channel = (int) (status & 0x0F) + 1;

    if (type == 0x90 || type == 0x80)
    {
        if (event.numBytes < 3)
            return;

        const auto note = (int) event.bytes[1];
        const auto velocity = (int) event.bytes[2];

        if (type == 0x90 && velocity > 0)
            startVoice (channel, note, velocity);
        else
            releaseVoices (channel, note);

        return;
    }

    if (type == 0xB0 && event.numBytes >= 3)
    {
        // 123 = all notes off, 120 = all sound off.
        if (event.bytes[1] == 123 || event.bytes[1] == 120)
            allNotesOffNow();
    }
}

void SamplerInstrument::applyMidiEvent (const juce::MidiMessageMetadata& metadata) noexcept
{
    if (metadata.data == nullptr || metadata.numBytes <= 0)
        return;

    PendingEvent event;
    event.numBytes = static_cast<std::uint8_t> (
        juce::jmin (maximumMidiBytes, metadata.numBytes));

    for (int i = 0; i < (int) event.numBytes; ++i)
        event.bytes[i] = metadata.data[i];

    applyQueuedEvent (event);
}

//==============================================================================
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

    // Kill the previous hit(s) in the group before the new one starts. The
    // fade is short (2 ms by default) so the cut is inaudible as a click but
    // faster than the 5 ms acceptance window.
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

        // Already fading faster (for example because of a choke): keep the
        // faster envelope.
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

//==============================================================================
void SamplerInstrument::renderVoices (juce::AudioBuffer<float>& buffer,
                                      int startSample, int numSamples) noexcept
{
    const int channels = buffer.getNumChannels();
    const int totalSamples = buffer.getNumSamples();

    if (channels <= 0 || numSamples <= 0)
        return;

    const int first = juce::jlimit (0, totalSamples, startSample);
    const int last = juce::jlimit (first, totalSamples, startSample + numSamples);

    if (last <= first)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = channels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int sample = first; sample < last; ++sample)
    {
        float leftSum = 0.0f;
        float rightSum = 0.0f;

        for (auto& voice : voices)
        {
            if (! voice.active)
                continue;

            const auto& source = *voice.sample;
            const int frames = source.audio.getNumSamples();

            if (frames <= 0 || voice.position >= (double) frames)
            {
                setVoiceInactive (voice);
                continue;
            }

            const int index = (int) std::floor (voice.position);
            const int next = juce::jmin (index + 1, frames - 1);
            const auto fraction = (float) (voice.position - (double) index);
            const int sourceChannels = source.audio.getNumChannels();

            const auto* sourceLeft = source.audio.getReadPointer (0);
            const auto* sourceRight = sourceChannels > 1 ? source.audio.getReadPointer (1)
                                                         : sourceLeft;

            const float leftSample = sourceLeft[index]
                                     + fraction * (sourceLeft[next] - sourceLeft[index]);
            const float rightSample = sourceRight[index]
                                      + fraction * (sourceRight[next] - sourceRight[index]);

            const float envelope = voice.gain;
            leftSum += leftSample * voice.leftGain * envelope;
            rightSum += rightSample * voice.rightGain * envelope;

            if (voice.fadeStep > 0.0f)
            {
                voice.gain -= voice.fadeStep;

                if (voice.gain <= 0.0f)
                {
                    voice.gain = 0.0f;
                    setVoiceInactive (voice);
                }
            }

            voice.position += voice.ratio;
        }

        if (right != nullptr)
        {
            left[sample] += leftSum;
            right[sample] += rightSum;
        }
        else
        {
            left[sample] += (leftSum + rightSum) * monoPanSum;
        }
    }
}

void SamplerInstrument::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    // Events queued by noteOn()/noteOff() belong at the start of this block.
    drainQueuedEvents();

    int rendered = 0;

    auto event = midi.cbegin();
    const auto end = midi.cend();

    while (rendered < numSamples)
    {
        int nextEventPosition = numSamples;

        if (event != end)
        {
            const auto position = (*event).samplePosition;

            if (position > rendered)
                nextEventPosition = juce::jmin (numSamples, position);
        }

        if (nextEventPosition > rendered)
        {
            renderVoices (buffer, rendered, nextEventPosition - rendered);
            rendered = nextEventPosition;
        }

        // An event timestamped at or past the end of the block belongs to a
        // later block; do not let it sound early.
        if (rendered >= numSamples)
            break;

        bool handled = false;

        while (event != end && (*event).samplePosition <= rendered)
        {
            applyMidiEvent (*event);
            ++event;
            handled = true;
        }

        if (! handled)
        {
            // Defensive: an event with a negative position or an iterator that
            // did not advance must not stall the block. Render one sample and
            // continue; events are still checked again on the next iteration.
            renderVoices (buffer, rendered, 1);
            ++rendered;
        }
    }
}

} // namespace ihub::sampler
