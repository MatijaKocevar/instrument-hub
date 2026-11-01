#include "Fixtures.h"

#include "TestSupport.h"

#include <cmath>
#include <memory>
#include <string>

namespace SamplerTest
{

juce::File testDirectory (const juce::String& name)
{
    auto directory = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    directory.createDirectory();
    return directory;
}

juce::AudioBuffer<float> makeSine (int frames, double sampleRate, double frequency,
                                   double amplitude, int channels)
{
    juce::AudioBuffer<float> buffer (channels, frames);

    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        const auto channelScale = channel == 0 ? 1.0 : 0.5;

        for (int i = 0; i < frames; ++i)
            data[i] = (float) (amplitude * channelScale
                               * std::sin (twoPi * frequency * (double) i / sampleRate));
    }

    return buffer;
}

bool writeAudioFile (const juce::File& file,
                     juce::AudioFormat& format,
                     const juce::AudioBuffer<float>& buffer,
                     double sampleRate,
                     int bitsPerSample,
                     juce::AudioFormatWriterOptions::SampleFormat sampleFormat)
{
    file.deleteFile();

    auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream().release());

    if (stream == nullptr)
        return false;

    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withNumChannels (buffer.getNumChannels())
                             .withBitsPerSample (bitsPerSample)
                             .withSampleFormat (sampleFormat);

    auto writer = format.createWriterFor (stream, options);

    if (writer == nullptr)
        return false;

    const bool ok = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    writer.reset();

    return ok;
}

void saveFloatWav (const juce::File& file,
                   const juce::AudioBuffer<float>& buffer,
                   double sampleRate,
                   const std::string& label)
{
    juce::WavAudioFormat format;
    const bool ok = writeAudioFile (file, format, buffer, sampleRate, 32,
                                    juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    check (ok, label + ": " + file.getFileName().toStdString());
}

} // namespace SamplerTest
