#include "Tempo.h"

#include "Fixtures.h"

namespace AudioTest::ClockOut
{

void runTempo()
{
    std::cout << "\n-- tempo and device formats --" << std::endl;

    {
        AudioEngine::Transport transport;
        transport.prepare (48000.0, 256);
        transport.setBpm (60.0);
        transport.start();

        AudioEngine::ClockOut clock;
        const auto events = render (clock, transport, 48000, 256);

        check (countClocks (events) == 24,
               "one second at 60 BPM emits 24 clocks");

        const double worst = worstJitterSamples (events, 48000.0 * 60.0 / (60.0 * 24.0));
        check (worst < 48.0, "60 BPM clocks stay within 1 ms");
    }

    {
        AudioEngine::Transport transport;
        transport.prepare (44100.0, 512);
        transport.setBpm (120.0);
        transport.start();

        AudioEngine::ClockOut clock;
        const auto events = render (clock, transport, 441000, 512);

        check (countClocks (events) == 480,
               "10 s at 44.1 kHz still emits 480 clocks");

        const double samplesPerTick = 44100.0 * 60.0 / (120.0 * 24.0);
        const double worst = worstJitterSamples (events, samplesPerTick);

        std::cout << "44.1 kHz worst jitter: " << worst << " samples ("
                  << worst * 1000.0 / 44100.0 << " ms)" << std::endl;

        check (worst < 44.1, "44.1 kHz jitter is under 1 ms (44.1 samples)");
    }

    {
        AudioEngine::Transport transport;
        transport.prepare (96000.0, 1024);
        transport.setBpm (180.0);
        transport.start();

        AudioEngine::ClockOut clock;
        const auto events = render (clock, transport, 96000, 1024);

        check (countClocks (events) == 72,
               "one second at 180 BPM emits 72 clocks (3 beats x 24)");

        const double worst = worstJitterSamples (events, 96000.0 * 60.0 / (180.0 * 24.0));
        check (worst < 96.0, "180 BPM at 96 kHz stays within 1 ms");
    }

    // A tempo change between blocks preserves tick continuity.
    {
        AudioEngine::Transport transport;
        transport.prepare (48000.0, 128);
        transport.setBpm (120.0);
        transport.start();

        AudioEngine::ClockOut clock;
        auto first = render (clock, transport, 48000, 128);
        check (countClocks (first) == 48, "120 BPM first second is 48 clocks");

        transport.setBpm (60.0);
        auto second = render (clock, transport, 48000, 128);
        check (countClocks (second) == 24, "a tempo change to 60 BPM halves the clock rate");

        bool continuous = true;
        std::int64_t expected = 49;

        for (const auto& event : second)
        {
            if (event.type != AudioEngine::ClockOut::Type::clock)
                continue;

            if (event.tick != expected)
                continuous = false;

            ++expected;
        }

        check (continuous && expected == 73,
               "the tick counter continues across the tempo change");
    }
}

} // namespace AudioTest::ClockOut
