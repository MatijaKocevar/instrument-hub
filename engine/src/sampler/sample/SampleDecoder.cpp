#include "SampleDecoder.h"

#include "../SampleLoader.h"
#include "SampleResampler.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

namespace ihub::sampler::sample_detail
{

std::unique_ptr<SampleData> decodeFile (const juce::File& file,
                                        double engineRate,
                                        juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return nullptr;
    }

    if (! isSupportedEngineRate (engineRate))
    {
        error = "Unsupported engine sample rate";
        return nullptr;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + file.getFileName();
        return nullptr;
    }

    const auto sourceFrames = reader->lengthInSamples;
    const auto sourceRate = reader->sampleRate;

    if (sourceFrames <= 0 || ! std::isfinite (sourceRate) || sourceRate < 1.0)
    {
        error = "Audio file is empty or has an invalid sample rate";
        return nullptr;
    }

    if (sourceFrames > SampleLoader::maximumSourceFrames
        || (double) sourceFrames / sourceRate > SampleLoader::maximumDurationSeconds)
    {
        error = "Audio file is longer than the "
                + juce::String (SampleLoader::maximumDurationSeconds, 0) + " second limit";
        return nullptr;
    }

    const auto sourceChannels = (int) reader->numChannels;
    const int channels = juce::jlimit (1, 2, sourceChannels);

    juce::AudioBuffer<float> decoded (channels, (int) sourceFrames);
    decoded.clear();

    if (! reader->read (&decoded, 0, (int) sourceFrames, 0, true, true))
    {
        error = "Failed to read audio data from " + file.getFileName();
        return nullptr;
    }

    auto destination = std::make_unique<SampleData>();

    destination->audio = resampleBuffer (decoded, sourceRate, engineRate);
    destination->sampleRate = engineRate;
    destination->sourceSampleRate = sourceRate;
    destination->sourceFrames = sourceFrames;
    destination->sourceChannels = sourceChannels;
    destination->name = file.getFileName();
    destination->path = file.getFullPathName();

    if (destination->audio.getNumSamples() <= 0 || destination->audio.getNumChannels() <= 0)
    {
        error = "Resampled audio is empty";
        return nullptr;
    }

    return destination;
}

}
