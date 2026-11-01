#include "Queue.h"

#include "Fixtures.h"

namespace AudioTest::ClockIn
{

void runQueue()
{
    std::cout << "\n-- non-real-time bytes and queue overflow --" << std::endl;

    AudioEngine::ClockIn clockIn;

    check (! clockIn.pushRealtime (0x90, 0.0), "a note-on byte is ignored");
    check (! clockIn.pushRealtime (0xB0, 0.0), "a control-change byte is ignored");
    check (! clockIn.pushRealtime (0xF0, 0.0), "a SysEx start byte is ignored");
    check (clockIn.pushRealtime (0xF8, 0.0), "a clock byte is accepted");
    check (clockIn.getPendingEventCount() == 1, "the accepted byte is queued");
    check (clockIn.clearQueue() == 1, "clearQueue reports the pending event");
    check (clockIn.getPendingEventCount() == 0, "the queue is empty again");

    // Fill the queue: one slot is always kept free for the SPSC invariant.
    const int capacity = AudioEngine::ClockIn::queueCapacity;
    int accepted = 0;

    for (int index = 0; index < capacity + 16; ++index)
    {
        if (clockIn.pushRealtime (0xF8, static_cast<double> (index) * 0.02))
            ++accepted;
    }

    check (accepted == capacity - 1,
           "the queue accepts exactly capacity-1 events");
    check (clockIn.getDroppedEventCount() == 17,
           "the overflow is counted as dropped events");
    check (clockIn.getPendingEventCount() == capacity - 1,
           "every accepted event is pending");

    Fixture fixture;
    for (int block = 0; block < 64; ++block)
        clockIn.process (fixture.transport, kBlockSize);

    check (clockIn.getPendingEventCount() == 0,
           "process() drains the queue completely");
}

} // namespace AudioTest::ClockIn
