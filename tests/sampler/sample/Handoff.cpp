#include "Handoff.h"

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
using SamplerTest::saveFloatWav;
using ihub::sampler::SampleLoader;

void runHandoffChecks (const juce::File& directory)
{
    {
        const auto file = directory.getChildFile ("async-44k1.wav");
        saveFloatWav (file,
                      SamplerTest::makeSine (22050, standardRate, sineFrequency),
                      standardRate,
                      "fixture write (32-bit float WAV)");

        SampleLoader loader;
        check (loader.prepare (engineRate), "loader accepts 48 kHz");
        check (! loader.prepare (0.0), "loader rejects a zero sample rate");
        check (std::abs (loader.getEngineSampleRate() - engineRate) < 1.0e-9,
               "rejected rate leaves the engine rate unchanged");

        juce::String error;
        const auto requestId = loader.loadAsync (file, error);
        check (requestId > 0, "background load returns a request id");
        check (loader.waitForIdle (10000), "background load finishes");

        SampleLoader::Event event;
        check (loader.takeEvent (event), "background load reports an event");
        check (event.requestId == requestId, "event carries the request id");
        check (event.success, "event reports success");
        check (event.error.isEmpty(), "success event has no error");

        check (loader.getHandoff().hasPendingPublish(), "sample is published for hand-off");
        check (loader.current() == nullptr, "nothing is installed before the audio thread polls");

        const auto* sample = loader.poll();
        check (sample != nullptr, "audio-thread poll installs the sample");
        check (loader.current() == sample, "installed sample is current");
        check (loader.poll() == nullptr, "a second poll returns nothing new");

        if (sample != nullptr)
        {
            check (sample->audio.getNumSamples() == 24000, "async: length is 24000");
            checkPitch (*sample, sineFrequency, "async", 2.0);
        }

        allocCount::total.store (0);
        allocCount::enabled.store (true);

        for (int i = 0; i < 20000; ++i)
        {
            (void) loader.poll();
            (void) loader.current();
        }

        allocCount::enabled.store (false);
        check (allocCount::total.load() == 0,
               "poll/current allocate nothing (" + std::to_string (allocCount::total.load())
                   + " allocations)");

        loader.collectGarbage();
    }

    {
        const auto firstFile = directory.getChildFile ("replace-1.wav");
        const auto secondFile = directory.getChildFile ("replace-2.wav");
        saveFloatWav (firstFile,
                      SamplerTest::makeSine (2400, engineRate, 500.0),
                      engineRate,
                      "fixture write (32-bit float WAV)");
        saveFloatWav (secondFile,
                      SamplerTest::makeSine (4800, engineRate, 2000.0),
                      engineRate,
                      "fixture write (32-bit float WAV)");

        SampleLoader loader;
        juce::String error;

        loader.loadAsync (firstFile, error);
        loader.waitForIdle (10000);
        const auto* first = loader.poll();

        SampleLoader::Event event;
        while (loader.takeEvent (event)) {}

        loader.loadAsync (secondFile, error);
        loader.waitForIdle (10000);
        const auto* second = loader.poll();

        check (first != nullptr && second != nullptr && first != second,
               "a replacement publishes a different sample");
        check (loader.getHandoff().getNumRetired() == 1,
               "the replaced sample is queued for retirement (not freed on the audio thread)");
        check (loader.getHandoff().getNumOverflows() == 0, "retirement queue did not overflow");

        loader.collectGarbage();
        check (loader.getHandoff().getNumRetired() == 0, "replacement garbage collected");
    }
}

}
