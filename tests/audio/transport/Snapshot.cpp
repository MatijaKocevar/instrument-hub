#include "Snapshot.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runSnapshot()
{
    std::cout << "\n-- snapshot --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 256);
    transport.setBpm (128.0);
    transport.start();
    advance (transport, 12000, 128);

    const auto snapshot = transport.snapshot();

    check (snapshot.playing, "snapshot reports the playing state");
    check (snapshot.bpm == 128.0, "snapshot reports the tempo");
    check (nearlyEqual (snapshot.ppq, 12000.0 * 128.0 / (60.0 * 48000.0), 1.0e-12),
           "snapshot reports PPQ");
    check (snapshot.positionSamples == 12000, "snapshot reports the frame position");
    check (snapshot.sampleRate == 48000.0 && snapshot.maximumBlockSize == 256,
           "snapshot reports the device format");
    check (snapshot.syncSource == AudioEngine::Transport::SyncSource::internal,
           "snapshot reports the sync source");
}

}
