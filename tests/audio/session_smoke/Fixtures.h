#pragma once

#include "../TestSupport.h"

#include "Session.h"
#include "audio/RtGuard.h"

#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace AudioTest::SessionSmoke
{

using AudioTest::check;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;
constexpr int kNumSlots = 64;
constexpr double kDiscontinuityLimitDbfs = -60.0;
constexpr double kMinimumRmsDbfs = -60.0;

ihub::SessionConfig makeConfig (float fadeMilliseconds);

void checkReport (const ihub::SessionSwitchReport& report, const std::string& label);

double renderCallbackRms (ihub::Session& session);

} // namespace AudioTest::SessionSmoke
