#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Api
{

//==============================================================================
/** What `Discovery` publishes on the LAN. */
struct DiscoveryService
{
    /** DNS-SD instance label, e.g. "Instrument Hub". */
    std::string instanceName = "Instrument Hub";

    /** Host label without the `.local` suffix.

        When empty, `start()` derives `instrument-hub-<computer name>`.
    */
    std::string hostLabel;

    /** IPv4 address published in the A record. Empty or loopback means no A
        record is sent, so the instance is listed but not resolvable.
    */
    std::string address;

    /** TCP port of the control API. */
    int port = 8080;

    /** TXT key/value pairs. Clients use `path` to find the remote route. */
    std::vector<std::pair<std::string, std::string>> txt {
        { "path", "/remote" },
        { "version", "0.1.0" }
    };
};

//==============================================================================
/**
    A minimal DNS-SD/mDNS responder (T102).

    Advertises `_instrumenthub._tcp.local` on 224.0.0.251:5353 while LAN mode
    is enabled, so `avahi-browse -t _instrumenthub._tcp` (and any other mDNS
    browser) lists the engine without a configuration step.

    The responder is self-contained — no avahi library, no daemon dependency.
    It joins the mDNS multicast group, announces the PTR/SRV/TXT/A record set,
    answers PTR/SRV/TXT/A queries and sends a TTL=0 goodbye on `stop()`. Binding
    port 5353 uses SO_REUSEADDR/SO_REUSEPORT so it coexists with avahi-daemon.

    The responder only ever runs on a background thread and never touches the
    audio path. On platforms without BSD sockets it starts nothing and reports
    an error; LAN mode still works.
*/
class Discovery
{
public:
    Discovery();
    ~Discovery();

    Discovery (const Discovery&) = delete;
    Discovery& operator= (const Discovery&) = delete;

    //==============================================================================
    static constexpr const char* serviceType = "_instrumenthub._tcp";
    static constexpr const char* defaultHostPrefix = "instrument-hub";

    /** Starts advertising `service`.

        Stops any previous advertisement first. Returns false when the mDNS
        socket cannot be created/bound (for example when port 5353 is
        unavailable); `getLastError()` then explains why. A failed start never
        breaks the control API.
    */
    bool start (DiscoveryService service);
    void stop();

    bool isRunning() const noexcept;

    /** The service currently advertised, including the derived host label. */
    DiscoveryService getService() const;

    std::string getLastError() const;

    //==============================================================================
    /** The DNS-SD service type without the `.local` suffix. */
    static std::string getServiceType();

    /** Lower-cases, replaces non `[a-z0-9-]` characters with `-`, collapses
        runs of `-`, trims them and truncates to one DNS label (63 bytes).
        Returns `fallback` when nothing usable remains.
    */
    static std::string sanitiseLabel (const std::string& text,
                                      const std::string& fallback);

    /** `instrument-hub-<sanitised computer name>` (or just the prefix). */
    static std::string defaultHostLabel();

private:
    /** Stops and joins the worker. Caller holds `lifecycleMutex`. */
    void stopLocked();

    void run (int socketFd, DiscoveryService service);

    mutable std::mutex lifecycleMutex;
    mutable std::mutex stateMutex;

    DiscoveryService advertised;
    std::string lastError;

    std::thread worker;
    std::atomic<bool> running { false };
};

} // namespace Api
