#pragma once

#include <string>

namespace httplib {
class Server;
}

namespace Api::Routes {

bool isUiRequestPath(const std::string& path);

void registerUiRoutes(httplib::Server& server);

}  // namespace Api::Routes
