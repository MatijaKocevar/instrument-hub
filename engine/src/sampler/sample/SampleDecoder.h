#pragma once

#include "../SampleData.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace ihub::sampler::sample_detail
{

std::unique_ptr<SampleData> decodeFile (const juce::File& file,
                                        double engineSampleRate,
                                        juce::String& error);

}
