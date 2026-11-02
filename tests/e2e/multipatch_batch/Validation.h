#pragma once

#include "multipatch_batch/Record.h"

#include <vector>

namespace MultiPatch
{

struct ValidationSummary
{
    int covered = 0;
    int restored = 0;
    int uniqueStates = 0;
};

ValidationSummary validateBatches (const std::vector<BatchRecord>& records);

}
