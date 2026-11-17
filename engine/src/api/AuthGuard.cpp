#include "AuthGuard.h"

#include "Pairing.h"
#include "api/UiRoutes.h"

#include <httplib.h>

#include <string_view>
#include <utility>

namespace Api {

namespace {
bool isLoopbackAddress(const std::string& address) {
    return address == "127.0.0.1" || address == "::1" || address == "[::1]" ||
           address == "::ffff:127.0.0.1" || address.rfind("127.", 0) == 0;
}

bool isWebSocketPath(const std::string& path) {
    return path == "/meters" || path == "/events" || path == "/midi-input";
}
}  // namespace

AuthGuard::AuthGuard(PairingManager& pairingToUse, std::string tokenToUse)
    : pairing(pairingToUse), token(std::move(tokenToUse)) {}

void AuthGuard::setToken(std::string newToken) {
    const std::lock_guard<std::mutex> lock(mutex);
    token = std::move(newToken);
}

bool AuthGuard::hasToken() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return !token.empty();
}

bool AuthGuard::authorized(const httplib::Request& request) const {
    if (request.path == "/pair") return true;

    if (request.path.rfind("/pairing", 0) == 0) return isLoopbackAddress(request.remote_addr);

    if (Routes::isUiRequestPath(request.path)) return true;

    if (isLoopbackAddress(request.remote_addr)) return true;

    std::string expected;
    bool lanEnabled = false;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        expected = token;
        lanEnabled = pairing.isLanEnabled();
    }

    if (expected.empty() && !lanEnabled) return true;

    auto supplied = request.get_header_value("X-Instrument-Hub-Token");

    if (supplied.empty()) {
        const auto authorization = request.get_header_value("Authorization");
        constexpr std::string_view bearer = "Bearer ";

        if (authorization.rfind(bearer, 0) == 0) supplied = authorization.substr(bearer.size());
    }

    if (supplied.empty() && isWebSocketPath(request.path))
        supplied = request.get_param_value("token");

    if (!expected.empty() && supplied == expected) return true;

    return lanEnabled && pairing.validateToken(supplied);
}

}  // namespace Api
