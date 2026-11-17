#pragma once

#include "controller/MappingModel.h"
#include "project/SetFile.h"

#include <map>

namespace ihub::app {

void importMappingPatches(const Controller::SetMapping& document,
                          std::map<int, ihub::project::SlotConfig>& slots);

}
