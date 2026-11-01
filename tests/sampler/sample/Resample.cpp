#include "Resample.h"

#include "../Fixtures.h"
#include "../TestSupport.h"
#include "Assertions.h"
#include "Constants.h"

#include "sampler/SampleLoader.h"

#include <cmath>
#include <string>

namespace SampleTest
{

using SamplerTest::check;
using SamplerTest::number;
using SamplerTest::saveFloatWav;
using ihub::sampler::SampleLoader;

void runResampleChecks (const juce::File& directory)
{
    {
        const auto file = directory.getChildFile ("tone-44k1.wav");
        const auto source = SamplerTest::makeSine (22050, standardRate, sineFrequency);
        saveFloatWav (file, source, standardRate, "fixture write (32-bit float WAV)");

        juce::String error;
        const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

        check (sample != nullptr, "44.1 kHz WAV loads");

        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 24000,
                   "44.1 kHz: 22050 frames become 24000 frames (got "
                       + std::to_string (sample->audio.getNumSamples()) + ")");
            check (std::abs (sample->sampleRate - engineRate) < 1.0e-9,
                   "44.1 kHz: output is at 48 kHz");
            check (std::abs (sample->sourceSampleRate - standardRate) < 1.0e-9,
                   "44.1 kHz: source rate preserved");
            check (std::abs (sample->getLengthSeconds() - 0.5) < 1.0e-9,
                   "44.1 kHz: output duration is 0.5 s");
            checkPitch (*sample, sineFrequency, "44.1 kHz", 2.0);

            const auto peak = sample->audio.getMagnitude (0, 0, sample->audio.getNumSamples());
            check (std::abs (peak - sineAmplitude) < 0.02,
                   "44.1 kHz: amplitude preserved (peak " + number (peak) + ")");
        }
    }

    {
        struct Case { int sourceFrames; int expectedFrames; };
        const Case cases[] = { { 1000, 1088 }, { 4410, 4800 }, { 8820, 9600 },
                               { 44100, 48000 }, { 22050, 24000 } };

        for (const auto& testCase : cases)
        {
            const auto file = directory.getChildFile ("length-"
                                                       + std::to_string (testCase.sourceFrames)
                                                       + ".wav");
            saveFloatWav (file,
                          SamplerTest::makeSine (testCase.sourceFrames, standardRate,
                                                 sineFrequency),
                          standardRate,
                          "fixture write (32-bit float WAV)");

            juce::String error;
            const auto sample = SampleLoader::loadBlocking (file, engineRate, error);
            const int actual = sample != nullptr ? sample->audio.getNumSamples() : -1;

            check (actual == testCase.expectedFrames,
                   "length: " + std::to_string (testCase.sourceFrames)
                       + " frames at 44.1 kHz -> " + std::to_string (testCase.expectedFrames)
                       + " at 48 kHz (got " + std::to_string (actual) + ")");
        }

        for (const double frequency : { 220.0, 440.0, 1000.0, 4000.0, 8000.0 })
        {
            const auto file = directory.getChildFile ("pitch-" + number (frequency, 0) + ".wav");
            saveFloatWav (file,
                          SamplerTest::makeSine (22050, standardRate, frequency, 0.4),
                          standardRate,
                          "fixture write (32-bit float WAV)");

            juce::String error;
            const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

            if (sample == nullptr)
            {
                check (false, "pitch case " + number (frequency, 0) + " Hz loads");
                continue;
            }

            checkPitch (*sample, frequency, "pitch " + number (frequency, 0) + " Hz",
                        juce::jmax (3.0, frequency * 0.005));
        }
    }

    {
        const auto source = SamplerTest::makeSine (96000, 96000.0, sineFrequency);
        const auto file = directory.getChildFile ("tone-96k.wav");
        saveFloatWav (file, source, 96000.0, "fixture write (32-bit float WAV)");

        juce::String error;
        const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

        check (sample != nullptr, "96 kHz WAV loads");
        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 48000,
                   "96 kHz: 96000 frames become 48000 frames (got "
                       + std::to_string (sample->audio.getNumSamples()) + ")");
            checkPitch (*sample, sineFrequency, "96 kHz downsampling", 1.0);
        }
    }
}

}
