import type { ReactElement } from "react";

import { isPairingCode } from "./model";

interface PairingTokenSectionProps
{
    readonly token: string | null;
    readonly code: string;
    readonly lanEnabled: boolean;
    readonly busy: boolean;
    readonly onPair?: ((code: string) => void) | undefined;
}

/**
    This browser's own pairing state.

    Shows whether a token is stored and, while LAN mode is on with a live code,
    offers the one-click self-pair action.
*/
export function PairingTokenSection ({
    token,
    code,
    lanEnabled,
    busy,
    onPair,
}: PairingTokenSectionProps): ReactElement
{
    const canPairSelf = lanEnabled && isPairingCode (code) && token === null
                        && ! busy && onPair !== undefined;

    return (
        <>
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
        </>
    );
}
