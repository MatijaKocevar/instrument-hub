#pragma once

#include "dsp/drive/Drive.h"

#include <map>
#include <string>

namespace AudioEngine
{

    bool applyDriveParameters (ihub::dsp::Drive& drive,
                               const std::map<std::string, float>& parameters,
                               std::string& error);

    std::map<std::string, float> readDriveParameters (const ihub::dsp::Drive& drive);

}
