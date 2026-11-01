#include "Stress.h"

#include "../TestSupport.h"
#include "Constants.h"

#include "sampler/SampleLoader.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace SampleTest
{

using SamplerTest::check;
using ihub::sampler::SampleData;
using ihub::sampler::SampleHandoff;

void runStressChecks()
{
    SampleHandoff handoff;
    std::atomic<bool> producerDone { false };
    constexpr int publications = 200;

    std::thread producer ([&handoff, &producerDone]
    {
        for (int i = 0; i < publications; ++i)
        {
            auto sample = std::make_unique<SampleData>();
            sample->audio.setSize (1, 64);
            sample->audio.clear();
            sample->audio.setSample (0, 0, (float) i);
            sample->sampleRate = engineRate;
            sample->name = "stress-" + juce::String (i);

            handoff.publish (std::move (sample));
            std::this_thread::sleep_for (std::chrono::microseconds (200));
        }

        producerDone.store (true);
    });

    int observed = 0;

    while (! producerDone.load())
    {
        if (handoff.poll() != nullptr)
        {
            ++observed;

            if ((observed & 15) == 0)
                handoff.collectGarbage();
        }
    }

    while (handoff.poll() != nullptr)
    {
        ++observed;
        handoff.collectGarbage();
    }

    producer.join();

    check (observed > 0 && observed <= publications,
           "stress: consumer observed " + std::to_string (observed)
               + " of " + std::to_string (publications) + " publications");
    check (handoff.getNumOverflows() == 0, "stress: retirement queue never overflowed");
    handoff.collectGarbage();
    check (handoff.getNumRetired() == 0, "stress: everything reclaimed");
}

}
