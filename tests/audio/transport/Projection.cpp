#include "Projection.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runProjection()
{
    std::cout << "\n-- within-block projection --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);

    check (transport.getPpqAtSampleOffset (128) == 0.0,
           "a stopped transport does not project forward");

    transport.start();
    transport.setPpq (4.0);

    const double projected = transport.getPpqAtSampleOffset (128);
    check (nearlyEqual (projected, 4.0 + 128.0 / 24000.0, 1.0e-12),
           "128 frames at 120 BPM project 128/24000 of a quarter note");
    check (transport.getPpq() == 4.0,
           "projecting does not advance the transport");
}

}
