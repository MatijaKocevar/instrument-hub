#pragma once

#include "../TestSupport.h"

#include "audio/TapTempo.h"
#include "audio/Transport.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace AudioTest::TapTempo
{

using AudioTest::check;
using AudioTest::nearlyEqual;

double fourTaps (const double* seconds);

}
