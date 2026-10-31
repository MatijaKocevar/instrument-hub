import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import {
    formatCodeLifetime,
    isPairingCode,
    pairingPayload,
    spacedPairingCode,
} from "./model";
import { QrCodeImage } from "./QrCodeImage";
import "./pairing.css";

export interface PairingPanelProps
{
    pairing: ControlPairingState | null;
    token: string | null;
    error?: string | null;
    busy?: boolean;
    loading?: boolean;
    onToggleLan?: (enabled: boolean) => void;
    onForget?: () => void;
    onPair?: (code: string) => void;
}

/**
    Desktop pairing card.

    LAN mode is off by default: with it off the engine only listens on
    loopback and needs no token. Turning it on makes the control API require a
    token, shows the live six-digit code and renders the QR a tablet scans.
*/
export function PairingPanel ({
    pairing,
    token,
    error = null,
    busy = false,
    loading = false,
    onToggleLan,
    onForget,
    onPair,
}: PairingPanelProps): ReactElement
{
    const lanEnabled = pairing?.lanEnabled === true;
    const code = pairing?.code ?? "";
    const pairedClients = pairing?.pairedClients ?? 0;
    const canForget = lanEnabled && pairedClients > 0 && !busy;
    const canToggle = !busy && !loading && onToggleLan !== undefined;
    const canPairSelf = lanEnabled && isPairingCode (code) && token === null
                        && !busy && onPair !== undefined;

    return (
        <section
            className="pairing"
            aria-label="Tablet pairing"
            data-testid="pairing"
            data-lan={lanEnabled ? "true" : "false"}
            data-paired-clients={pairedClients}
        >
            <header className="pairing__header">
                <div>
                    <p className="pairing__eyebrow">Remote</p>
                    <h2 className="pairing__title">Tablet pairing</h2>
                </div>
                <span
                    className={`status-pill pairing__status${lanEnabled ? " pairing__status--on" : ""}`}
                    data-testid="pairing-lan-status"
                >
                    {lanEnabled ? "LAN mode on" : "LAN mode off"}
                </span>
            </header>

            <p className="pairing__lede">
                {lanEnabled
                    ? "Scan this code with the tablet camera. The code is single-use and expires; a fresh one appears here automatically."
                    : "LAN mode is off, so only this computer can reach the engine. Turn it on to pair a tablet on the same network."}
            </p>

            <div className="pairing__actions">
                <button
                    className={`pairing__toggle${lanEnabled ? " pairing__toggle--on" : ""}`}
                    type="button"
                    data-testid="lan-toggle"
                    aria-pressed={lanEnabled}
                    disabled={!canToggle}
                    onClick={() => { onToggleLan?. (! lanEnabled); }}
                >
                    {lanEnabled ? "Turn off LAN mode" : "Turn on LAN mode"}
                </button>

                <span
                    className="pairing__token-state"
                    data-testid="pairing-token-state"
                    data-paired={token === null ? "false" : "true"}
                >
                    {token === null ? "This browser is not paired" : "This browser is paired"}
                </span>

                {canPairSelf
                    ? <button
                          className="pairing__pair-self"
                          type="button"
                          data-testid="pairing-pair-self"
                          onClick={() => { onPair?.(code); }}
                      >
                          Pair this browser
                      </button>
                    : null}
            </div>

            {error !== null
                ? <p className="pairing__error" role="alert" data-testid="pairing-error">
                      {error}
                  </p>
                : null}

            {loading && pairing === null
                ? <p className="pairing__loading" aria-busy="true" data-testid="pairing-loading">
                      Reading the pairing state…
                  </p>
                : null}

            {lanEnabled && pairing !== null
                ? <div className="pairing__body">
                      <div className="pairing__qr-column">
                          <QrCodeImage value={pairingPayload (pairing)} size={228} />
                          <p className="pairing__endpoint" data-testid="pairing-endpoint">
                              {pairing.address}:{pairing.port}
                          </p>
                      </div>

                      <div className="pairing__details">
                          <p className="pairing__label">Pairing code</p>
                          <p
                              className="pairing__code"
                              data-testid="pairing-code"
                              data-code={code}
                          >
                              {spacedPairingCode (code)}
                          </p>

                          <dl className="pairing__meta">
                              <div>
                                  <dt>Expires in</dt>
                                  <dd data-testid="pairing-expiry">
                                      {formatCodeLifetime (pairing.codeExpiresInMs)}
                                  </dd>
                              </div>
                              <div>
                                  <dt>Paired tablets</dt>
                                  <dd data-testid="pairing-clients">{pairedClients}</dd>
                              </div>
                              <div>
                                  <dt>Remote URL</dt>
                                  <dd>
                                      <code data-testid="pairing-payload">{pairing.payload}</code>
                                  </dd>
                              </div>
                          </dl>

                          <button
                              className="pairing__forget"
                              type="button"
                              data-testid="pairing-forget"
                              disabled={!canForget}
                              onClick={() => { onForget?. (); }}
                          >
                              Forget paired tablets
                          </button>
                      </div>
                  </div>
                : null}
        </section>
    );
}
