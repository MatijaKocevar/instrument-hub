#include "Failure.h"

#include "../TestSupport.h"
#include "Constants.h"

#include "sampler/SampleLoader.h"

namespace SampleTest
{

using SamplerTest::check;
using ihub::sampler::SampleLoader;

void runFailureChecks (const juce::File& directory)
{
    juce::String error;
    const auto missing = SampleLoader::loadBlocking (
        directory.getChildFile ("does-not-exist.wav"), engineRate, error);
    check (missing == nullptr, "missing file returns nullptr");
    check (error.isNotEmpty(), "missing file fills an error");

    const auto garbage = directory.getChildFile ("garbage.wav");
    garbage.replaceWithText ("this is not an audio file");

    error.clear();
    const auto unsupported = SampleLoader::loadBlocking (garbage, engineRate, error);
    check (unsupported == nullptr, "garbage file returns nullptr");
    check (error.isNotEmpty(), "garbage file fills an error");

    SampleLoader loader;
    const auto badId = loader.loadAsync (garbage, error);
    check (badId > 0, "garbage file still queues a background request");
    check (loader.waitForIdle (5000), "failed background request finishes");

    SampleLoader::Event event;
    bool sawFailure = false;

    while (loader.takeEvent (event))
    {
        if (event.requestId == badId && ! event.success && event.error.isNotEmpty())
            sawFailure = true;
    }

    check (sawFailure, "failed background request reports a failure event");
    check (loader.poll() == nullptr, "failed background request publishes nothing");
}

}
