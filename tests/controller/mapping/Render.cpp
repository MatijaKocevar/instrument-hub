#include "Render.h"

#include "Fixtures.h"

#include <algorithm>
#include <cmath>

namespace MappingTest
{

SineSource::SineSource (double frequencyToUse, float amplitudeToUse, double phaseOffset)
    : frequency (frequencyToUse),
      amplitude (amplitudeToUse),
      initialPhase (phaseOffset)
{
}

void SineSource::prepare (double newSampleRate, int)
{
    phase = initialPhase;
    increment = juce::MathConstants<double>::twoPi * frequency / newSampleRate;
}

void SineSource::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto value = static_cast<float> (amplitude * std::sin (phase));
        phase += increment;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.getWritePointer (channel)[frame] = value;
    }
}

void renderBlock (AudioEngine::SlotSwitcher& switcher,
                  juce::MidiBuffer& midi,
                  RenderStats& stats,
                  float& previousSample)
{
    const bool rendered = switcher.process (kBlockSize, midi);

    if (! rendered)
        stats.alwaysRendered = false;

    const auto& output = switcher.getOutputBuffer();
    double energy = 0.0;

    for (int frame = 0; frame < kBlockSize; ++frame)
    {
        const auto sample = output.getSample (0, frame);
        energy += static_cast<double> (sample) * static_cast<double> (sample);

        const double delta = std::abs (static_cast<double> (sample)
                                       - static_cast<double> (previousSample));
        stats.maxDelta = std::max (stats.maxDelta, delta);
        previousSample = sample;

        if (! std::isfinite (sample))
            stats.alwaysRendered = false;
    }

    const double rms = std::sqrt (energy / static_cast<double> (kBlockSize));
    stats.minRms = std::min (stats.minRms, rms);

    if (rms < kDropoutRmsFloor)
        ++stats.silentBlocks;

    ++stats.blocks;
}

}
