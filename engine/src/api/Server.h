#pragma once

#include "Discovery.h"
#include "Pairing.h"
#include "StateStore.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Api
{

class StateBroadcast;

//==============================================================================
// HTTP + WebSocket server
//==============================================================================

/**
    The local control server: one `cpp-httplib` HTTP listener plus a WebSocket
    endpoint for meters.

    Endpoints:

    - `GET  /state`
    - `POST /arm`
    - `GET  /settings`
    - `POST /settings`
    - `POST /transport`
    - `PATCH /patches/{slot}`
    - `POST /patches/{slot}/source`
    - `POST /patches/{slot}/editor`
    - `DELETE /patches/{slot}/editor`
    - `POST /patches/{slot}/state/capture`
    - `GET  /patches/{slot}/fx`
    - `POST /patches/{slot}/fx`
    - `PUT  /patches/{slot}/fx`
    - `PATCH/DELETE /patches/{slot}/fx/{index}`
    - `POST /patches/{slot}/fx/reorder`
    - `GET  /audio`
    - `POST /audio`
    - `POST /audio/measure`
    - `GET  /plugins`
    - `POST /plugins/rescan`
    - `GET  /midi`
    - `POST /midi/inputs`
    - `POST /midi/pad-test`
    - `GET  /meters` (WebSocket upgrade)
    - `GET  /events` (WebSocket upgrade, live state mirror)
    - `GET  /pairing` (loopback only)
    - `POST /pairing/lan` (loopback only)
    - `POST /pairing/revoke` (loopback only)
    - `POST /pair`

    Binding defaults to `127.0.0.1:8080` (plan/DECISIONS.md). Authentication is
    off unless `Options::authToken` is non-empty or LAN mode is enabled; when
    enabled, requests must carry `X-Instrument-Hub-Token: <token>` or
    `Authorization: Bearer <token>`. A pairing token obtained from `POST /pair`
    is accepted alongside the configured token.

    LAN mode is off by default. Enabling it rebinds the listener to all
    interfaces, makes authentication mandatory and starts a pairing code. The
    QR payload and code are local-only management endpoints; the phone/tablet
    only needs `POST /pair`.

    Meter frames are produced once per `meterHz` on a dedicated thread and
    coalesced for every connected client. The audio buffer rate is irrelevant.
*/
class Server
{
public:
    struct Options
    {
        std::string address = "127.0.0.1";
        int port = 8080;

        /** Empty disables authentication. */
        std::string authToken;

        /** Start with LAN mode already enabled (rebinds to all interfaces). */
        bool lanMode = false;

        /** Broadcast rate for `/meters`; clamped to 1..120. */
        int meterHz = 30;
    };

    explicit Server (StateStore& state);
    Server (StateStore& state, Options options);
    ~Server();

    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;

    //==============================================================================
    // Lifecycle

    /** Binds and starts the listener and the meter thread.

        Returns false when the address/port cannot be bound (for example when
        another instance already owns it).
    */
    bool start();
    void stop();

    bool isRunning() const noexcept;
    bool isListening() const noexcept;

    /** The actual bound port. Useful when `Options::port` is 0. */
    int getPort() const noexcept;
    std::string getAddress() const;

    //==============================================================================
    // Configuration

    /** Replaces the meter source. Safe before or after `start()`. */
    void setMeterProvider (MeterProvider provider);

    /** Installs the audio device backend behind `/audio` and `/audio/measure`.

        Call before `start()`; the handlers are copied into the route table.
        A missing provider makes `GET /audio` report a closed device, and the
        mutating endpoints answer `503`.
    */
    void setAudioHandlers (AudioStateProvider provider,
                           AudioApplyHandler apply,
                           AudioMeasureHandler measure);

    /** Installs the plug-in inventory behind `/plugins` (T122).

        Call before `start()`; the handlers are copied into the route table.
        The inventory object they close over must outlive this server. Without
        them `GET /plugins` returns an empty list and `POST /plugins/rescan`
        answers `503`.
    */
    void setPluginHandlers (PluginInventoryProvider provider,
                            PluginRescanHandler rescan);

    /** Installs the MIDI device enumeration and controller controls behind
        `/midi` (T126). Call before `start()`; the objects they close over must
        outlive this server. Without a provider `GET /midi` reports no inputs
        and the mutating endpoints answer `503`.
    */
    void setMidiHandlers (MidiStateProvider provider,
                          MidiInputHandler enableInput,
                          MidiPadTestHandler padTest);

    /** Enables/disables token auth. Takes effect on the next request. */
    void setAuthToken (std::string token);
    bool hasAuthToken() const;

    //==============================================================================
    // LAN pairing (T101)

    /** The pairing manager behind `/pairing` and `/pair`. */
    PairingManager& getPairing();
    const PairingManager& getPairing() const;

    /** The mDNS responder that advertises `_instrumenthub._tcp` in LAN mode
        (T102). It runs only while the listener is bound to all interfaces.
    */
    Discovery& getDiscovery();
    const Discovery& getDiscovery() const;

    /** The live state mirror behind the `/events` WebSocket (T103).

        A pad press, `POST /arm`, `POST /settings` or `POST /transport` all
        publish a frame; every connected client receives the newest state
        within a few milliseconds. The broadcaster starts and stops with the
        server.
    */
    StateBroadcast& getStateBroadcast();
    const StateBroadcast& getStateBroadcast() const;

    /** Turn LAN mode on/off.

        Enabling rebinds the listener from loopback to all interfaces, makes
        token auth mandatory and issues a pairing code. Disabling reverses both
        and revokes every issued token. Returns the state immediately after the
        toggle; the rebind itself happens on the server's control thread so the
        HTTP request that triggered it can finish.
    */
    PairingSnapshot setLanMode (bool enabled);
    bool isLanModeEnabled() const;

    /** Number of clients that received at least one meter frame. */
    std::uint64_t getMeterFrameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Api
