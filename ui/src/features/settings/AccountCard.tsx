import type { ReactElement } from "react";

import type { ConnectionPhase } from "../../state/store";

const PHASE_LABEL: Record<ConnectionPhase, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Connected",
    offline: "Offline",
};

interface AccountCardProps
{
    /** Latest connection phase for the control API. */
    readonly connection: ConnectionPhase;
    /** Base URL this browser talks to. */
    readonly baseUrl: string;
    /** Token held by this browser, or null when it has not paired. */
    readonly token: string | null;
    /** Tablets the engine currently trusts. */
    readonly pairedClients: number;
    /** True when revocation is allowed right now. */
    readonly canRevoke: boolean;
    readonly onForget?: (() => void) | undefined;
}

/**
    This browser's identity.

    There is deliberately no login system: the only identity here is the
    pairing token this browser already holds, and revoking is the single
    action.
*/
export function AccountCard ({
    connection,
    baseUrl,
    token,
    pairedClients,
    canRevoke,
    onForget,
}: AccountCardProps): ReactElement
{
    return (
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
    );
}
