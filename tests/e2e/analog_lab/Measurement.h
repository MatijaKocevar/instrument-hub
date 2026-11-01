#pragma once

#include "Session.h"
#include "analog_lab/AudioProbe.h"

#include <string>

namespace AnalogLab
{

struct Measurement
{
    AudioProbe audioA;
    AudioProbe audioB;
    double difference = 0.0;
    int peakLiveInstances = 0;
};

bool measurePass (ihub::Session& session,
                  const std::string& hashA,
                  const std::string& hashB,
                  const std::string& phase,
                  Measurement& measurement);

} // namespace AnalogLab
