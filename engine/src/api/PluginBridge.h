#pragma once

#include "Server.h"

namespace ihub::plugins
{
class PluginInventory;
}

namespace Api
{

/** Connects a `plugins::PluginInventory` to `GET /plugins` and
    `POST /plugins/rescan` (T122).

    The inventory must outlive the server: the route handlers hold a reference
    and call it on HTTP threads. Call before `Server::start()`.
*/
void attachPluginInventory (Server& server, ihub::plugins::PluginInventory& inventory);

} // namespace Api
