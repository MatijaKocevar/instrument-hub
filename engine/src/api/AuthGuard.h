#pragma once

#include <mutex>
#include <string>

namespace httplib
{
struct Request;
}

namespace Api
{

class PairingManager;

//==============================================================================
/**
    The authentication policy for the control API (T101).

    `Server` asks this object before every HTTP request and WebSocket upgrade,
    so the rules live in one place:

    - `POST /pair` is always reachable: it is how a client obtains its first
      token.
    - `/pairing...` is loopback-only, even for a paired client, because it
      exposes the live code. A tablet must not be able to mint more tokens.
    - Every other request is open while no token is configured and LAN mode is
      off. Once either is active, the request must carry
      `X-Instrument-Hub-Token` or `Authorization: Bearer <token>` matching the
      configured token or one issued by the pairing manager.

    Threading: `authorized()` runs on HTTP worker threads and may also be called
    while another thread updates the token; every path is mutex protected.
*/
class AuthGuard
{
public:
    explicit AuthGuard (PairingManager& pairing, std::string token = {});

    AuthGuard (const AuthGuard&) = delete;
    AuthGuard& operator= (const AuthGuard&) = delete;

    /** Replaces the configured token; empty disables the configured check. */
    void setToken (std::string token);

    /** True when a configured token is set (LAN mode may still force auth). */
    bool hasToken() const;

    /** True when `request` may reach its handler. */
    bool authorized (const httplib::Request& request) const;

private:
    PairingManager& pairing;

    mutable std::mutex mutex;
    std::string token;
};

} // namespace Api
