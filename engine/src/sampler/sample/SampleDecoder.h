#pragma once

#include "../SampleData.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub::sampler::sample_detail
{

/** Decodes one audio file and resamples it to `engineSampleRate`.

    `loadBlocking()` uses this directly; the background loader thread calls it
    for every queued job, so both paths share exactly one decode policy.
*/
std::unique_ptr<SampleData> decodeFile (const juce::File& file,
                                        double engineSampleRate,
                                        juce::String& error);

} // namespace ihub::sampler::sample_detail
