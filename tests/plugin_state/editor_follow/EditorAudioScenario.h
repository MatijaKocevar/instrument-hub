#pragma once

#include <functional>
#include <string>

namespace ihub
{
class Session;
}

namespace EditorAudioScenario
{

using Check = std::function<void (bool condition, const std::string& what)>;

void run (ihub::Session& session, int slot, const Check& check);

}
