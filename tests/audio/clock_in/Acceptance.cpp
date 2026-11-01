#include "Acceptance.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runAcceptance()
{
    std::cout << "\n-- acceptance: 120 BPM clock locks within 2 s --" << std::endl;

    Fixture fixture;
    Simulator simulator (kSampleRate, 120.0);
    simulator.start();

    constexpr std::int64_t twoSeconds = 96000;
    std::int64_t lockedAtFrame = -1;

    for (std::int64_t processed = 0; processed < twoSeconds; processed += kBlockSize)
    {
        simulator.advance (fixture.transport, kBlockSize);

        if (lockedAtFrame < 0 && simulator.clockIn.isLocked())
            lockedAtFrame = simulator.frameIndex();
    }

    const auto& clockIn = simulator.clockIn;

    std::cout << "locked after " << (lockedAtFrame < 0 ? -1.0 : lockedAtFrame / kSampleRate)
              << " s, estimate " << clockIn.getBpm() << " BPM, ppq " << clockIn.getPpq()
              << ", transport ppq " << fixture.transport.getPpq() << std::endl;

    check (lockedAtFrame > 0 && lockedAtFrame <= twoSeconds,
           "the clock locks within 2 s");
    check (nearlyEqual (clockIn.getBpm(), 120.0, 0.01),
           "the tempo estimate is 120 BPM");
    check (clockIn.isPlaying() && clockIn.isClockPresent(),
           "the follower reports a present clock that is playing");

    check (fixture.transport.isPlaying(),
           "the transport is rolling in external sync");
    check (fixture.transport.getSyncSource() == AudioEngine::Transport::SyncSource::external
               && fixture.transport.isExternalClockLocked(),
           "the transport adopted the external clock");
    check (nearlyEqual (fixture.transport.getBpm(), 120.0, 0.01),
           "the transport tempo is 120 BPM");

    check (nearlyEqual (fixture.transport.getPpq(), 4.0, tick),
           "the transport position is 4 quarter notes +/- one tick");
    check (std::abs (clockIn.getTickCount() - 95) <= 1,
           "95 ticks arrived in the first 2 s (96 x 24 PPQN per 2 s)");

    check (clockIn.getDroppedEventCount() == 0,
           "no clock event was dropped");
}

}
