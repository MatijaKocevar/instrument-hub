import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import type { ConnectionPhase } from "../../state/store";
import { AccountCard } from "./AccountCard";
import { AboutCard } from "./AboutCard";
import { PairingPanel } from "../pairing";
import { SettingsHeader } from "./SettingsHeader";

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
    card reports what the engine says it is running. This file is the layout.
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
    const canRevoke = lanEnabled && ! pairingBusy && onForget !== undefined;

    return (
        <section className="page settings" data-testid="settings-page">
            <SettingsHeader />

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
                <AccountCard
                    connection={connection}
                    baseUrl={baseUrl}
                    token={token}
                    pairedClients={pairedClients}
                    canRevoke={canRevoke}
                    onForget={onForget}
                />

                <AboutCard
                    application={application}
                    schemaVersion={schemaVersion}
                    uptimeMs={uptimeMs}
                />
            </div>
        </section>
    );
}
