#pragma once

#include "../TestSupport.h"

#include "audio/ClockOut.h"
#include "audio/Transport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace AudioTest::ClockOut
{

using AudioTest::check;
using AudioTest::nearlyEqual;

struct RecordingSink final : AudioEngine::MidiSink
{
    void sendRealTime (std::uint8_t statusByte) noexcept override
    {
        bytes.push_back (statusByte);
    }

    std::vector<std::uint8_t> bytes;
};

std::vector<AudioEngine::ClockOut::Event> render (AudioEngine::ClockOut& clock,
                                                  AudioEngine::Transport& transport,
                                                  std::int64_t frames,
                                                  int blockSize);

int countClocks (const std::vector<AudioEngine::ClockOut::Event>& events);

double worstJitterSamples (const std::vector<AudioEngine::ClockOut::Event>& events,
                           double samplesPerTick);

}
