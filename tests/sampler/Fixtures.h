#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <string>

namespace SamplerTest
{

juce::File testDirectory (const juce::String& name);

juce::AudioBuffer<float> makeSine (int frames, double sampleRate, double frequency,
                                   double amplitude = 0.5, int channels = 1);

bool writeAudioFile (const juce::File& file,
                     juce::AudioFormat& format,
                     const juce::AudioBuffer<float>& buffer,
                     double sampleRate,
                     int bitsPerSample,
                     juce::AudioFormatWriterOptions::SampleFormat sampleFormat =
                         juce::AudioFormatWriterOptions::SampleFormat::automatic);

void saveFloatWav (const juce::File& file,
                   const juce::AudioBuffer<float>& buffer,
                   double sampleRate,
                   const std::string& label = "fixture write");

}
