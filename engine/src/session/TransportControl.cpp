#include "TransportControl.h"

namespace ihub::session {

AudioEngine::FxTransport TransportControl::getFxTransport() const noexcept {
    const auto snapshot = transport.snapshot();

    AudioEngine::FxTransport result;
    result.playing = snapshot.playing;
    result.bpm = snapshot.bpm;
    result.ppq = snapshot.ppq;
    return result;
}

}  // namespace ihub::session
