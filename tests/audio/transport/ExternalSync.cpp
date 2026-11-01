#include "ExternalSync.h"

#include "Fixtures.h"

namespace AudioTest::Transport
{

void runExternalSync()
{
    std::cout << "\n-- external sync hook --" << std::endl;

    AudioEngine::Transport transport;
    transport.prepare (48000.0, 128);
    transport.setBpm (120.0);

    AudioEngine::Transport::ExternalSync update;
    update.locked = true;
    update.playing = true;
    update.bpm = 90.0;
    update.ppq = 4.0;
    update.positionSamples = 128000;

    check (! transport.applyExternalSync (update),
           "external updates are ignored while the source is internal");
    check (transport.getPpq() == 0.0 && transport.getBpm() == 120.0,
           "the ignored update changes nothing");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::external);
    check (transport.applyExternalSync (update),
           "a locked update is applied in external mode");

    check (transport.isPlaying() && transport.getBpm() == 90.0,
           "the external tempo and running state are adopted");
    check (nearlyEqual (transport.getPpq(), 4.0, 1.0e-12)
               && transport.getPositionSamples() == 128000,
           "the external position is adopted");
    check (transport.isExternalClockLocked(),
           "a locked update reports the clock as locked");

    advance (transport, 24000, 128);
    check (nearlyEqual (transport.getPpq(), 4.75, 1.0e-12),
           "the transport advances at the external tempo between updates");

    AudioEngine::Transport::ExternalSync lost;
    lost.locked = false;

    check (transport.applyExternalSync (lost),
           "a lost clock is accepted from the external source");
    check (! transport.isPlaying() && ! transport.isExternalClockLocked(),
           "a lost clock stops the transport and clears the lock");
    check (transport.getBpm() == 90.0 && nearlyEqual (transport.getPpq(), 4.75, 1.0e-12),
           "a lost clock holds tempo and position");

    const double held = transport.getPpq();
    advance (transport, 48000, 128);
    check (transport.getPpq() == held,
           "a stopped external transport does not advance");

    transport.setSyncSource (AudioEngine::Transport::SyncSource::internal);
    check (! transport.isExternalClockLocked(),
           "switching back to internal clears the external lock");
}

}
