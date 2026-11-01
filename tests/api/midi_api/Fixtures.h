#pragma once

#include "api/MidiBridge.h"
#include "api/Server.h"

#include <string>

namespace ApiTest::MidiApi
{

Api::MidiInputState makeInput (const std::string& id,
                               const std::string& name,
                               const std::string& role,
                               bool enabled);

Api::MidiStateSnapshot makeFakeMidi();

struct FakeBackend
{
    Api::MidiStateSnapshot midi = makeFakeMidi();
    int padTests = 0;

    bool setInput (const std::string& id, bool enabled, std::string& error);

    bool runPadTest (std::string& error);

    void attach (Api::Server& server);
};

}
