#include "Decode.h"

#include "../Fixtures.h"
#include "../Measurements.h"
#include "../TestSupport.h"
#include "Assertions.h"
#include "Constants.h"

#include "sampler/SampleLoader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace SampleTest
{

using SamplerTest::check;
using SamplerTest::meanOfMiddle;
using SamplerTest::number;
using SamplerTest::saveFloatWav;
using SamplerTest::writeAudioFile;
using ihub::sampler::SampleLoader;

void runDecodeChecks (const juce::File& directory)
{
    {
        const auto file = directory.getChildFile ("tone-48k.wav");
        const auto source = SamplerTest::makeSine (24000, engineRate, sineFrequency);
        saveFloatWav (file, source, engineRate, "fixture write (32-bit float WAV)");

        juce::String error;
        const auto sample = SampleLoader::loadBlocking (file, engineRate, error);

        check (sample != nullptr, "48 kHz WAV loads");

        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 24000,
                   "48 kHz: output length 24000 frames (got "
                       + std::to_string (sample->audio.getNumSamples()) + ")");
            check (sample->audio.getNumChannels() == 1, "48 kHz: mono stays mono");
            check (std::abs (sample->sampleRate - engineRate) < 1.0e-9,
                   "48 kHz: output sample rate is the engine rate");
            check (std::abs (sample->sourceSampleRate - engineRate) < 1.0e-9,
                   "48 kHz: source sample rate preserved");
            check (sample->sourceFrames == 24000, "48 kHz: source frame count preserved");

            double maxDifference = 0.0;

            for (int i = 0; i < 24000; ++i)
                maxDifference = juce::jmax (maxDifference,
                                            (double) std::abs (sample->audio.getSample (0, i)
                                                               - source.getSample (0, i)));

            const bool identicalBits =
                std::memcmp (sample->audio.getReadPointer (0),
                             source.getReadPointer (0),
                             (size_t) 24000 * sizeof (float)) == 0;

            check (identicalBits,
                   "48 kHz: equal-rate load is a bit-exact copy (max diff "
                       + number (maxDifference) + ")");
            checkPitch (*sample, sineFrequency, "48 kHz");
        }
    }

    {
        const auto source = SamplerTest::makeSine (22050, standardRate, sineFrequency);

        const auto aiff = directory.getChildFile ("tone.aiff");
        juce::AiffAudioFormat aiffFormat;
        check (writeAudioFile (aiff, aiffFormat, source, standardRate, 24),
               "fixture write (AIFF)");

        juce::String error;
        const auto aiffSample = SampleLoader::loadBlocking (aiff, engineRate, error);
        check (aiffSample != nullptr, "AIFF loads");
        if (aiffSample != nullptr)
        {
            check (aiffSample->audio.getNumSamples() == 24000, "AIFF: length is 24000");
            checkPitch (*aiffSample, sineFrequency, "AIFF", 2.0);
        }

        const auto flac = directory.getChildFile ("tone.flac");
        juce::FlacAudioFormat flacFormat;
        check (writeAudioFile (flac, flacFormat, source, standardRate, 24),
               "fixture write (FLAC)");

        error.clear();
        const auto flacSample = SampleLoader::loadBlocking (flac, engineRate, error);
        check (flacSample != nullptr, "FLAC loads");
        if (flacSample != nullptr)
        {
            check (flacSample->audio.getNumSamples() == 24000, "FLAC: length is 24000");
            checkPitch (*flacSample, sineFrequency, "FLAC", 2.0);
        }
    }

    {
        juce::AudioBuffer<float> dc (1, 22050);
        std::fill_n (dc.getWritePointer (0), 22050, 0.25f);

        const auto dcFile = directory.getChildFile ("dc.wav");
        saveFloatWav (dcFile, dc, standardRate, "fixture write (32-bit float WAV)");

        juce::String error;
        const auto dcSample = SampleLoader::loadBlocking (dcFile, engineRate, error);

        check (dcSample != nullptr, "DC file loads");
        if (dcSample != nullptr)
        {
            const auto measured = meanOfMiddle (dcSample->audio);
            check (std::abs (measured - 0.25) < 0.002,
                   "DC level preserved through resampling (" + number (measured, 4) + ")");
        }

        const auto stereoFile = directory.getChildFile ("stereo.wav");
        saveFloatWav (stereoFile,
                      SamplerTest::makeSine (22050, standardRate, sineFrequency,
                                             sineAmplitude, 2),
                      standardRate,
                      "fixture write (32-bit float WAV)");

        error.clear();
        const auto stereo = SampleLoader::loadBlocking (stereoFile, engineRate, error);

        check (stereo != nullptr, "stereo file loads");
        if (stereo != nullptr)
        {
            check (stereo->audio.getNumChannels() == 2, "stereo stays stereo");
            check (stereo->sourceChannels == 2, "stereo source channel count preserved");
            check (stereo->audio.getNumSamples() == 24000, "stereo: length is 24000");

            const auto left = stereo->audio.getRMSLevel (0, 0, stereo->audio.getNumSamples());
            const auto right = stereo->audio.getRMSLevel (1, 0, stereo->audio.getNumSamples());

            check (std::abs (right / left - 0.5) < 0.02,
                   "stereo: channel balance preserved (R/L " + number (right / left) + ")");
        }
    }
}

}
