import { ApiError } from "../api/client";
import {
    clearPairingToken,
    isPairingCode,
    savePairingToken,
    type TokenStorage,
} from "../features/pairing/model";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/** Store services pairing needs beyond the shared action context. */
export interface PairingServices
{
    /** Token persistence; null when this environment has no localStorage. */
    readonly storage: TokenStorage | null;
    /** Re-read the engine state after pairing changes the token. */
    readonly refresh: () => Promise<boolean>;
}

/**
    Manage LAN mode, the pairing code and this browser's token.

    Enabling LAN mode makes token auth mandatory. The desktop stays usable by
    pairing itself with the freshly issued code, so the user sees the QR and
    the live engine state at the same time.
*/
export class PairingActions
{
    constructor (private readonly context: ActionContext,
                 private readonly services: PairingServices) {}

    /** Turn LAN mode on/off. */
    toggleLan = async (enabled: boolean): Promise<boolean> =>
    {
        this.patch ({ pairingBusy: true, pairingError: null });

        try
        {
            let pairing = await this.context.api.setLanMode (enabled);
            let autoPairError: string | null = null;

            if (! enabled)
            {
                this.dropToken();
            }
            else if (this.context.api.token === null && isPairingCode (pairing.code))
            {
                // The listener rebinds to all interfaces right after the
                // toggle response. POST /pair may briefly see 503 or a closed
                // socket, so retry before giving up on the desktop's own token.
                for (let attempt = 0; attempt < 8 && this.context.api.token === null; attempt += 1)
                {
                    try
                    {
                        const result = await this.context.api.pair (pairing.code);
                        this.adoptToken (result.token);

                        try
                        {
                            pairing = await this.context.api.getPairing ();
                        }
                        catch
                        {
                            // The next poll picks the rotated code up.
                        }

                        autoPairError = null;
                        break;
                    }
                    catch (error)
                    {
                        autoPairError = messageOf (error);

                        const transient = ! (error instanceof ApiError)
                            || error.status === 503
                            || error.status === 0;

                        if (! transient)
                            break;

                        await new Promise (resolve => setTimeout (resolve, 120));
                    }
                }
            }

            this.patch ({
                pairing,
                pairingBusy: false,
                pairingError: autoPairError,
                unauthorized: false,
            });

            if (enabled)
                await this.services.refresh ();

            return true;
        }
        catch (error)
        {
            this.patch ({
                pairingBusy: false,
                pairingError: messageOf (error),
            });

            return false;
        }
    };

    /**
        Exchange a six-digit code for a token and keep it in this browser.

        Used by the tablet gate and by the manual fallback when the QR link is
        opened without a code.
    */
    pair = async (code: string): Promise<boolean> =>
    {
        this.patch ({ pairingBusy: true, pairingError: null });

        try
        {
            const result = await this.context.api.pair (code);
            this.adoptToken (result.token);

            let pairing = this.snapshot.pairing;

            try
            {
                pairing = await this.context.api.getPairing ();
            }
            catch
            {
                // The desktop-only endpoint is not readable from a tablet.
            }

            this.patch ({
                pairing,
                pairingBusy: false,
                pairingError: null,
                unauthorized: false,
                error: null,
            });

            await this.services.refresh ();
            return true;
        }
        catch (error)
        {
            this.patch ({ pairingBusy: false, pairingError: messageOf (error) });
            return false;
        }
    };

    /**
        Revoke every issued token, then restore this browser's own access so
        the desktop does not lock itself out of the engine it just secured.
    */
    forgetPairing = async (): Promise<boolean> =>
    {
        this.patch ({ pairingBusy: true, pairingError: null });

        try
        {
            let pairing = await this.context.api.revokePairing ();

            if (pairing.lanEnabled && isPairingCode (pairing.code))
            {
                const result = await this.context.api.pair (pairing.code);
                this.adoptToken (result.token);
                pairing = await this.context.api.getPairing ();
            }
            else
            {
                this.dropToken();
            }

            this.patch ({ pairing, pairingBusy: false, pairingError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ pairingBusy: false, pairingError: messageOf (error) });
            return false;
        }
    };

    private adoptToken (token: string): void
    {
        this.context.api.setToken (token);
        savePairingToken (token, this.services.storage);
    }

    private dropToken (): void
    {
        this.context.api.setToken (null);
        clearPairingToken (this.services.storage);
    }

    private get snapshot ()
    {
        return this.context.getSnapshot ();
    }

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
