#include "Assertions.h"

#include "../Fixtures.h"
#include "../Measurements.h"
#include "../TestSupport.h"

#include <cmath>

namespace SampleTest
{

void checkPitch (const ihub::sampler::SampleData& sample, double expected,
                 const std::string& label, double toleranceHz)
{
    const double measured = SamplerTest::estimateFrequency (sample.audio, sample.sampleRate);

    SamplerTest::check (std::abs (measured - expected) <= toleranceHz,
                        label + ": pitch " + SamplerTest::number (measured, 1) + " Hz (expected "
                            + SamplerTest::number (expected, 1) + " Hz)");
}

}
