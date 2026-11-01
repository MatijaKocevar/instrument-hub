#include "Check.h"
#include "analog_lab/AudioProbe.h"
#include "analog_lab/Constants.h"

#include <cmath>

namespace AnalogLab
{
using E2E::check;

double toDbfs (double linear)
{
    return linear > 0.0 ? 20.0 * std::log10 (linear) : -1000.0;
}

AudioProbe renderNote (ihub::Session& session, int slot)
{
    AudioProbe probe;

    if (session.getPluginInstrument (slot) == nullptr)
        return probe;

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer emptyMidi;

    for (int block = 0; block < kSettleBlocks; ++block)
    {
        buffer.clear();
        emptyMidi.clear();
        session.render (buffer, emptyMidi, kBlockSize);
    }

    check (session.pushMidi (juce::MidiMessage::noteOn (1, 60, 0.9f)),
           "a note-on is queued for slot " + std::to_string (slot));

    double sumSquares = 0.0;
    int frames = 0;

    for (int block = 0; block < kRenderBlocks; ++block)
    {
        buffer.clear();
        emptyMidi.clear();
        session.render (buffer, emptyMidi, kBlockSize);

        for (int frame = 0; frame < kBlockSize; ++frame)
        {
            const float sample = buffer.getSample (0, frame);
            const auto value = static_cast<double> (sample);
            sumSquares += value * value;
            probe.peak = juce::jmax (probe.peak, std::abs (value));
            probe.samples.push_back (sample);
            ++frames;
        }
    }

    probe.rms = frames > 0 ? std::sqrt (sumSquares / static_cast<double> (frames))
                           : 0.0;

    check (session.pushMidi (juce::MidiMessage::noteOff (1, 60)),
           "a note-off is queued for slot " + std::to_string (slot));

    for (int block = 0; block < kTailBlocks; ++block)
    {
        buffer.clear();
        emptyMidi.clear();
        session.render (buffer, emptyMidi, kBlockSize);
    }

    return probe;
}

double relativeDifference (const AudioProbe& first, const AudioProbe& second)
{
    const auto frames = juce::jmin (first.samples.size(), second.samples.size());

    if (frames == 0)
        return 0.0;

    double difference = 0.0;

    for (std::size_t index = 0; index < frames; ++index)
    {
        const auto delta = static_cast<double> (first.samples[index])
                         - static_cast<double> (second.samples[index]);
        difference += delta * delta;
    }

    const double differenceRms = std::sqrt (difference / static_cast<double> (frames));
    const double reference = juce::jmax (first.rms, second.rms);
    return reference > 0.0 ? differenceRms / reference : 0.0;
}

} // namespace AnalogLab
