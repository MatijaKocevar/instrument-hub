#pragma once

#include "pad_audio/PagesScenario.h"

#include <string>

namespace PadAudio
{

struct MeasurementRecord
{
    double renderRms = 0.0;
    double renderRmsDbfs = -1000.0;
    double switchDiscontinuityDbfs = -1000.0;
    double padSwitchMaxDelta = 0.0;
    std::string loopbackPlayback;
    std::string loopbackCapture;
    double loopbackFirstDbfs = -1000.0;
    double loopbackSecondDbfs = -1000.0;
};

void writePagesRecord (const PagesResult& pages);

void writeMeasurementRecord (const MeasurementRecord& record);

}
