#pragma once

#include "Session.h"

#include <functional>
#include <string>

namespace ConcurrentInstances
{

using Check = std::function<void (bool, const std::string&)>;

void runEditorCapture (ihub::Session& session, const Check& check);

} // namespace ConcurrentInstances
