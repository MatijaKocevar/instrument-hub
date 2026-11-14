#pragma once

#include "Server.h"

namespace ihub
{
    class Session;
}

namespace Api
{

    void attachSession (StateStore& state, Server& server, ihub::Session& session);

}
