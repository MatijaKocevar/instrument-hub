#pragma once

#include "Server.h"

namespace ihub::plugins
{
class PluginInventory;
}

namespace Api
{

void attachPluginInventory (Server& server, ihub::plugins::PluginInventory& inventory);

}
