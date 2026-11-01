#pragma once

#include "../TestSupport.h"

#include "audio/ClockIn.h"
#include "audio/Transport.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace AudioTest::ClockIn
{

using AudioTest::check;
using AudioTest::nearlyEqual;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

constexpr double tick = 1.0 / 24.0;

class Simulator
{
public:
    Simulator (double sampleRate, double tempo)
        : rate (sampleRate), bpmValue (tempo)
    {
    }

    void start()
    {
        clockIn.pushRealtime (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::start),
                              timestampOf (frame));
        running = true;
        nextTickFrame = static_cast<double> (frame) + tickFrames();
    }

    void stop()
    {
        clockIn.pushRealtime (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::stop),
                              timestampOf (frame));
    }

    void resume()
    {
        clockIn.pushRealtime (static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::continue_),
                              timestampOf (frame));
        running = true;
        nextTickFrame = static_cast<double> (frame) + tickFrames();
    }

    void setTempo (double tempo)
    {
        bpmValue = tempo;
        nextTickFrame = static_cast<double> (frame) + tickFrames();
    }

    void disappear() { running = false; }

    void setJitter (double amplitudeSeconds, std::uint32_t seed)
    {
        jitter = amplitudeSeconds;
        randomState = seed;
    }

    void advance (AudioEngine::Transport& transport, int block)
    {
        const auto end = frame + block;

        if (running)
        {
            while (nextTickFrame < static_cast<double> (end))
            {
                const double timestamp = nextTickFrame / rate + nextJitter();

                clockIn.pushRealtime (
                    static_cast<std::uint8_t> (AudioEngine::ClockIn::Type::clock),
                    timestamp);

                nextTickFrame += tickFrames();
            }
        }

        clockIn.process (transport, block);
        transport.process (block);
        frame = end;
    }

    void run (AudioEngine::Transport& transport, std::int64_t frames, int block)
    {
        while (frames > 0)
        {
            const int step = static_cast<int> (
                std::min<std::int64_t> (frames, static_cast<std::int64_t> (block)));

            advance (transport, step);
            frames -= step;
        }
    }

    std::int64_t frameIndex() const noexcept { return frame; }

    AudioEngine::ClockIn clockIn;

private:
    double tickFrames() const noexcept { return rate * 60.0 / (bpmValue * 24.0); }

    double timestampOf (std::int64_t atFrame) const noexcept
    {
        return static_cast<double> (atFrame) / rate;
    }

    double nextJitter() noexcept
    {
        if (jitter <= 0.0)
            return 0.0;

        randomState = randomState * 1664525u + 1013904223u;
        const double unit = static_cast<double> ((randomState >> 8) & 0xFFFFu) / 65535.0;

        return (unit * 2.0 - 1.0) * jitter;
    }

    double rate;
    double bpmValue;
    double jitter = 0.0;
    std::uint32_t randomState = 1;
    std::int64_t frame = 0;
    double nextTickFrame = 0.0;
    bool running = false;
};

struct Fixture
{
    AudioEngine::Transport transport;

    Fixture()
    {
        transport.prepare (kSampleRate, kBlockSize);
        transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    }
};

}
