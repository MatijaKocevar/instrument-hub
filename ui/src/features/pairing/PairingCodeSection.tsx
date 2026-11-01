import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import { formatCodeLifetime, spacedPairingCode } from "./model";

interface PairingCodeSectionProps
{
    readonly pairing: ControlPairingState;
    readonly canForget: boolean;
    readonly onForget?: (() => void) | undefined;
}

export function PairingCodeSection ({
    pairing,
    canForget,
    onForget,
}: PairingCodeSectionProps): ReactElement
{
    const code = pairing.code;
    const pairedClients = pairing.pairedClients;

    return (
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
                disabled={! canForget}
                onClick={() => { onForget?. (); }}
            >
                Forget paired tablets
            </button>
        </div>
    );
}
