#include "Fixtures.h"

namespace AudioTest::ClockOut
{

std::vector<AudioEngine::ClockOut::Event> render (AudioEngine::ClockOut& clock,
                                                  AudioEngine::Transport& transport,
                                                  std::int64_t frames,
                                                  int blockSize)
{
    std::vector<AudioEngine::ClockOut::Event> events;
    events.reserve (static_cast<std::size_t> (frames / blockSize + 64));

    std::vector<AudioEngine::ClockOut::Event> drained (64);

    while (frames > 0)
    {
        const int block = static_cast<int> (
            std::min<std::int64_t> (frames, static_cast<std::int64_t> (blockSize)));

        clock.process (transport, block);

        for (;;)
        {
            const int count = clock.drain (drained.data(), static_cast<int> (drained.size()));

            for (int index = 0; index < count; ++index)
                events.push_back (drained[static_cast<std::size_t> (index)]);

            if (count < static_cast<int> (drained.size()))
                break;
        }

        transport.process (block);
        frames -= block;
    }

    return events;
}

int countClocks (const std::vector<AudioEngine::ClockOut::Event>& events)
{
    return static_cast<int> (std::count_if (events.begin(), events.end(),
                                            [] (const auto& event)
                                            {
                                                return event.type == AudioEngine::ClockOut::Type::clock;
                                            }));
}

double worstJitterSamples (const std::vector<AudioEngine::ClockOut::Event>& events,
                           double samplesPerTick)
{
    double worst = 0.0;

    for (const auto& event : events)
    {
        if (event.type != AudioEngine::ClockOut::Type::clock)
            continue;

        const double ideal = static_cast<double> (event.tick) * samplesPerTick;
        worst = std::max (worst, std::abs (static_cast<double> (event.transportSample) - ideal));
    }

    return worst;
}

} // namespace AudioTest::ClockOut
