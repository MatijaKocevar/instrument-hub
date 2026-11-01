#include "RenderHit.h"

#include <cmath>
#include <vector>

namespace KitTest
{

juce::AudioBuffer<float> renderHit (const ihub::sampler::ResolvedHit& hit)
{
    if (! hit.isValid())
        return {};

    const auto& source = *hit.sample;
    const auto ratio = hit.playbackRatio > 0.0 ? hit.playbackRatio : 1.0;
    const int outputFrames =
        (int) std::ceil ((double) source.audio.getNumSamples() / ratio);

    juce::AudioBuffer<float> output (2, juce::jmax (1, outputFrames));
    output.clear();

    const auto leftGain = hit.leftGain();
    const auto rightGain = hit.rightGain();
    const int sourceFrames = source.audio.getNumSamples();
    const int sourceChannels = source.audio.getNumChannels();

    for (int n = 0; n < output.getNumSamples(); ++n)
    {
        const double position = (double) n * ratio;
        const auto index = (int) std::floor (position);

        if (index < 0 || index >= sourceFrames)
            break;

        const auto fraction = (float) (position - (double) index);
        const auto nextIndex = juce::jmin (index + 1, sourceFrames - 1);

        const auto interpolate = [&] (int channel)
        {
            const auto* data = source.audio.getReadPointer (juce::jmin (channel, sourceChannels - 1));
            return data[index] + fraction * (data[nextIndex] - data[index]);
        };

        const auto left = interpolate (0);
        const auto right = sourceChannels > 1 ? interpolate (1) : left;

        output.setSample (0, n, left * leftGain);
        output.setSample (1, n, right * rightGain);
    }

    return output;
}

} // namespace KitTest
