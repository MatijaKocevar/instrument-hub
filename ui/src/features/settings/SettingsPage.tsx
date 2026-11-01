import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import type { ConnectionPhase } from "../../state/store";
import { PairingPanel } from "../pairing";

const PHASE_LABEL: Record<ConnectionPhase, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Connected",
    offline: "Offline",
};

function formatUptime (milliseconds: number | null): string
{
    if (milliseconds === null)
        return "—";

    const totalSeconds = Math.max (0, Math.floor (milliseconds / 1000));
    const minutes = Math.floor (totalSeconds / 60);
    const seconds = totalSeconds % 60;

    if (minutes >= 60)
    {
        const hours = Math.floor (minutes / 60);

        return `${String (hours)} h ${String (minutes % 60)} min`;
    }

    return minutes > 0 ? `${String (minutes)} min ${String (seconds)} s` : `${String (seconds)} s`;
}

export interface SettingsPageProps
{
    /** Latest `GET /pairing`, or null before the first successful request. */
    readonly pairing: ControlPairingState | null;
    /** Token held by this browser, or null when it has not paired. */
    readonly token: string | null;
    readonly pairingError?: string | null;
    readonly pairingBusy?: boolean;
    readonly connection: ConnectionPhase;
    readonly baseUrl: string;
    readonly application?: { readonly name: string; readonly version: string } | null;
    readonly schemaVersion?: number | null;
    readonly uptimeMs?: number | null;
    readonly onToggleLan?: (enabled: boolean) => void;
    readonly onForget?: () => void;
    readonly onPair?: (code: string) => void;
}

/**
    Settings & Account.

    The pairing card owns LAN mode and the live code; the account card names
    the browser's own token and exposes the single revoke action; the about
    card reports what the engine says it is running. There is deliberately no
    login system: the only identity here is the pairing token this browser
    already holds.
*/
export function SettingsPage ({
    pairing,
    token,
    pairingError = null,
    pairingBusy = false,
    connection,
    baseUrl,
    application = null,
    schemaVersion = null,
    uptimeMs = null,
    onToggleLan,
    onForget,
    onPair,
}: SettingsPageProps): ReactElement
{
    const lanEnabled = pairing?.lanEnabled === true;
    const pairedClients = pairing?.pairedClients ?? 0;
    const version = application?.version ?? "0.1.0";
    const canRevoke = lanEnabled && ! pairingBusy && onForget !== undefined;

    return (
        <section className="page settings" data-testid="settings-page">
            <header className="page__header">
                <p className="page__eyebrow">Settings &amp; Account</p>
                <h2 className="page__title">Settings &amp; Account</h2>
                <p className="page__lede">
                    Pair a tablet on your local network, manage this browser's access,
                    and check which engine build is running.
                </p>
            </header>

            <PairingPanel
                pairing={pairing}
                token={token}
                error={pairingError}
                busy={pairingBusy}
                loading={pairing === null}
                {...(onToggleLan === undefined ? {} : { onToggleLan })}
                {...(onForget === undefined ? {} : { onForget })}
                {...(onPair === undefined ? {} : { onPair })}
            />

            <div className="settings__grid">
                <section
                    className="settings__card"
                    aria-labelledby="settings-account-title"
                    data-testid="settings-account"
                >
                    <header className="settings__head">
                        <p className="settings__eyebrow">Account</p>
                        <h3 className="settings__title" id="settings-account-title">
                            This browser
                        </h3>
                    </header>

                    <dl className="settings__meta">
                        <div>
                            <dt>Connection</dt>
                            <dd data-testid="settings-connection" data-phase={connection}>
                                {PHASE_LABEL[connection]}
                            </dd>
                        </div>
                        <div>
                            <dt>Control API</dt>
                            <dd>
                                <code data-testid="settings-endpoint">{baseUrl}</code>
                            </dd>
                        </div>
                        <div>
                            <dt>Pairing token</dt>
                            <dd
                                data-testid="settings-token-state"
                                data-paired={token === null ? "false" : "true"}
                            >
                                {token === null ? "Not paired" : "Stored in this browser"}
                            </dd>
                        </div>
                        <div>
                            <dt>Paired tablets</dt>
                            <dd data-testid="settings-paired-clients">{pairedClients}</dd>
                        </div>
                    </dl>

                    <button
                        className="settings__button"
                        type="button"
                        data-testid="settings-revoke-token"
                        disabled={! canRevoke}
                        onClick={() => { onForget?. (); }}
                    >
                        Revoke pairing tokens
                    </button>
                    <p className="settings__hint">
                        Revoking signs out every paired tablet. This desktop re-pairs
                        itself automatically while LAN mode stays on.
                    </p>
                </section>

                <section
                    className="settings__card"
                    aria-labelledby="settings-about-title"
                    data-testid="settings-about"
                >
                    <header className="settings__head">
                        <p className="settings__eyebrow">About</p>
                        <h3 className="settings__title" id="settings-about-title">
                            Instrument Hub
                        </h3>
                    </header>

                    <dl className="settings__meta">
                        <div>
                            <dt>Application</dt>
                            <dd data-testid="settings-app-name">
                                {application?.name ?? "Instrument Hub"}
                            </dd>
                        </div>
                        <div>
                            <dt>Version</dt>
                            <dd data-testid="settings-version">v{version}</dd>
                        </div>
                        <div>
                            <dt>State schema</dt>
                            <dd data-testid="settings-schema">
                                {schemaVersion === null ? "—" : schemaVersion}
                            </dd>
                        </div>
                        <div>
                            <dt>Uptime</dt>
                            <dd data-testid="settings-uptime">{formatUptime (uptimeMs)}</dd>
                        </div>
                    </dl>

                    <p className="settings__hint">
                        Audio workspace for instruments, controller and set. The engine
                        owns the audio path; this window only controls it.
                    </p>
                </section>
            </div>
        </section>
    );
}
