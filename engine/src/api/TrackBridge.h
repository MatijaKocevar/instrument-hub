#pragma once

namespace Api {

class StateStore;

}

namespace ihub {
class Session;
}

namespace Api {

void attachTrackState(StateStore& state, ihub::Session& session);

}
