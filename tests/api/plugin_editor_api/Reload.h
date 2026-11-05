#pragma once

#include "Session.h"

#include <httplib.h>

namespace ApiTest::PluginEditorApi
{

void runReload (ihub::Session& session, httplib::Client& client);

} // namespace ApiTest::PluginEditorApi
