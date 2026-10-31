#pragma once

// The canonical definition lives in `engine/src/instruments/Instrument.h`
// (T030). This shim keeps the older `audio/Instrument.h` include path working
// for code written before the instrument layer got its own directory.
#include "../instruments/Instrument.h"
