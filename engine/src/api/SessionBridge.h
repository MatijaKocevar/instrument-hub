#pragma once

#include "Server.h"

namespace ihub
{
class Session;
}

namespace Api
{

/**
    Connects a composed `ihub::Session` to the control API.

    After this call the HTTP surface drives the real graph instead of mirrors:

    - `POST /arm` publishes the slot into `SlotSwitcher` (`Session::armSlot`),
      captures the outgoing patch's plugin state and closes any editor that is
      not bound to the newly armed patch,
    - `POST /transport` starts/stops the session transport, sets the tempo and
      seeks it, and `GET /transport` / `/state` read the transport's live
      sample-accurate position,
    - `GET /state` reports each slot's actual crossfade state and gain,
    - `/meters` reports the peak of each slot's real output and of the master
      bus, sampled from the audio thread through lock-free atomics.

    The session (and the store) must outlive the server; the server holds
    references to both. Call before `Server::start()` so the first meter frame
    already reads the engine.
*/
void attachSession (StateStore& state, Server& server, ihub::Session& session);

} // namespace Api
