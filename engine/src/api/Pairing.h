#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Api
{

//==============================================================================
/** What `GET /pairing` publishes to the desktop UI.

    The pairing code is only meaningful while LAN mode is enabled: with LAN mode
    off the engine listens on loopback only and needs no authentication.
*/
struct PairingSnapshot
{
    bool lanEnabled = false;

    /** Host/IP advertised in the QR payload. Empty when it cannot be found. */
    std::string address;
    int port = 8080;

    /** Six decimal digits, or empty when LAN mode is off. */
    std::string code;

    /** Remaining lifetime of `code`, in milliseconds. */
    double codeExpiresInMs = 0.0;

    /** Payload encoded in the QR code, e.g.
        `http://192.168.1.20:8080/remote?code=123456`.
    */
    std::string payload;

    /** Number of clients currently holding a pairing token. */
    int pairedClients = 0;
};

//==============================================================================
/**
    LAN mode and pairing tokens (T101).

    The manager owns two pieces of state:

    - a six-digit pairing code, issued on demand and rotated after a successful
      pairing, after `codeLifetime`, and whenever LAN mode is toggled;
    - the set of tokens handed out by `pair()`.

    None of this touches the audio thread; every method takes the manager's own
    mutex and is safe from HTTP worker threads.

    LAN mode is deliberately off by default. Enabling it makes the control API
    require a token; the server rebinds to all interfaces separately.
*/
class PairingManager
{
public:
    static constexpr int codeDigits = 6;
    static constexpr std::chrono::minutes codeLifetime { 5 };

    explicit PairingManager (std::string advertisedAddress = {}, int port = 8080);

    //==============================================================================
    /** Replace the advertised host and port used by the QR payload.

        An empty address asks the manager to discover the first non-loopback
        IPv4 address of this machine.
    */
    void setEndpoint (std::string advertisedAddress, int port);

    std::string getAdvertisedAddress() const;

    //==============================================================================
    /** Turn LAN mode on/off.

        Turning it off clears the code and revokes every token. Returns true when
        the state changed.
    */
    bool setLanEnabled (bool enabled);

    bool isLanEnabled() const;

    //==============================================================================
    /** A consistent snapshot, issuing a fresh code when LAN mode is on and the
        current one is missing or expired.
    */
    PairingSnapshot snapshot();

    /** Exchange a code for a token. On failure `error` explains why and `token`
        is untouched.
    */
    bool pair (const std::string& code,
               std::string& token,
               std::string& error,
               std::string clientName = {});

    /** True when `token` is one this manager issued and has not revoked. */
    bool validateToken (const std::string& token) const;

    /** Revoke every token; returns how many were removed. */
    std::size_t revokeAll();

    std::size_t pairedClientCount() const;

    //==============================================================================
    static std::string discoverLanAddress();
    static bool isValidCode (const std::string& code);

private:
    void issueCodeLocked();
    std::string makePayloadLocked() const;

    static std::string randomDigits (int count);
    static std::string randomToken();

    mutable std::mutex mutex;

    bool lanEnabled = false;
    std::string advertisedAddress;
    int port = 8080;

    std::string code;
    std::chrono::steady_clock::time_point codeExpiry;

    /** token -> optional client label. */
    std::unordered_map<std::string, std::string> tokens;
};

} // namespace Api
