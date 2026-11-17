#pragma once

#include "SessionConfig.h"
#include "project/SetFile.h"

namespace ihub::app {

void applyProjectSlot(const ihub::project::SlotConfig& source, ihub::SessionSlotConfig& slot);

}
