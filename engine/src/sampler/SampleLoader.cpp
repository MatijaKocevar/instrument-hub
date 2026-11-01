#include "SampleLoader.h"

#include "sample/SampleDecoder.h"
#include "sample/SampleResampler.h"

#include <algorithm>
#include <utility>

namespace ihub::sampler
{

SampleLoader::SampleLoader()
    : juce::Thread ("ihub-sample-loader")
{
    startThread (juce::Thread::Priority::low);
}

SampleLoader::~SampleLoader()
{
    signalThreadShouldExit();
    wakeUp.signal();
    stopThread (-1);

    const std::lock_guard<std::mutex> lock (queueMutex);
    jobs.clear();
}

bool SampleLoader::prepare (double newEngineSampleRate) noexcept
{
    if (! sample_detail::isSupportedEngineRate (newEngineSampleRate))
        return false;

    engineSampleRate.store (newEngineSampleRate, std::memory_order_release);
    return true;
}

double SampleLoader::getEngineSampleRate() const noexcept
{
    return engineSampleRate.load (std::memory_order_acquire);
}

SampleLoader::Ptr SampleLoader::loadBlocking (const juce::File& file,
                                              double engineSampleRate,
                                              juce::String& error)
{
    error.clear();
    auto decoded = sample_detail::decodeFile (file, engineSampleRate, error);

    if (decoded == nullptr)
        return nullptr;

    return Ptr (std::move (decoded));
}

int SampleLoader::loadAsync (const juce::File& file, SampleHandoff& destination, juce::String& error)
{
    error.clear();

    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFullPathName();
        return 0;
    }

    const auto requestId = nextRequestId.fetch_add (1, std::memory_order_relaxed);

    pending.fetch_add (1, std::memory_order_acq_rel);

    {
        const std::lock_guard<std::mutex> lock (queueMutex);

        if ((int) jobs.size() >= maximumQueuedJobs)
        {
            pending.fetch_sub (1, std::memory_order_acq_rel);
            error = "Sample loader queue is full";
            return 0;
        }

        Job job;
        job.id = requestId;
        job.file = file;
        job.engineSampleRate = getEngineSampleRate();
        job.destination = &destination;
        jobs.push_back (std::move (job));
    }

    wakeUp.signal();
    return requestId;
}

int SampleLoader::loadAsync (const juce::File& file, juce::String& error)
{
    return loadAsync (file, handoff, error);
}

void SampleLoader::finishJob (int id,
                              const juce::File& file,
                              std::unique_ptr<SampleData> sample,
                              juce::String error,
                              SampleHandoff& destination)
{
    Event event;
    event.requestId = id;
    event.path = file.getFullPathName();
    event.success = sample != nullptr;
    event.error = std::move (error);

    if (sample != nullptr)
        destination.publish (std::move (sample));

    pushEvent (std::move (event));
    pending.fetch_sub (1, std::memory_order_acq_rel);
}

void SampleLoader::pushEvent (Event event)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        events[start1] = std::move (event);
        eventFifo.finishedWrite (1);
    }
    else if (size2 > 0)
    {
        events[start2] = std::move (event);
        eventFifo.finishedWrite (1);
    }
    else
    {
        droppedEvents.fetch_add (1, std::memory_order_relaxed);
    }
}

bool SampleLoader::takeEvent (Event& event)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    eventFifo.prepareToRead (1, start1, size1, start2, size2);

    if (size1 <= 0 && size2 <= 0)
        return false;

    const auto index = size1 > 0 ? start1 : start2;
    event = std::move (events[(size_t) index]);
    events[(size_t) index] = Event {};
    eventFifo.finishedRead (1);

    return true;
}

bool SampleLoader::waitForIdle (int timeoutMs)
{
    const auto start = juce::Time::getMillisecondCounterHiRes();

    while (isBusy())
    {
        if (timeoutMs >= 0
            && juce::Time::getMillisecondCounterHiRes() - start >= (double) timeoutMs)
        {
            return false;
        }

        juce::Thread::sleep (1);
    }

    return true;
}

void SampleLoader::run()
{
    while (! threadShouldExit())
    {
        Job job;

        {
            const std::lock_guard<std::mutex> lock (queueMutex);

            if (! jobs.empty())
            {
                job = std::move (jobs.front());
                jobs.pop_front();
            }
        }

        if (job.destination == nullptr)
        {
            wakeUp.wait (100);
            continue;
        }

        juce::String error;
        auto sample = sample_detail::decodeFile (job.file, job.engineSampleRate, error);

        finishJob (job.id, job.file, std::move (sample), std::move (error), *job.destination);
    }
}

}
