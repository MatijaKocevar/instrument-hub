import type { ControlPairingState } from "../../api/client";

/** localStorage key for the pairing token. */
export const PAIRING_TOKEN_KEY = "instrument-hub.pairing.token";

/** The minimum surface of `localStorage` this feature needs. */
export interface TokenStorage
{
    getItem (key: string): string | null;
    setItem (key: string, value: string): void;
    removeItem (key: string): void;
}

function defaultStorage (): TokenStorage | null
{
    if (typeof localStorage === "undefined")
        return null;

    return localStorage;
}

/** Read the stored token, or null when this browser has not paired. */
export function loadPairingToken (storage: TokenStorage | null = defaultStorage ()): string | null
{
    if (storage === null)
        return null;

    try
    {
        const value = storage.getItem (PAIRING_TOKEN_KEY);

        return value !== null && value.length > 0 ? value : null;
    }
    catch
    {
        return null;
    }
}

/** Persist a token after a successful pairing. */
export function savePairingToken (token: string,
                                  storage: TokenStorage | null = defaultStorage ()): void
{
    if (storage === null || token.length === 0)
        return;

    try
    {
        storage.setItem (PAIRING_TOKEN_KEY, token);
    }
    catch
    {
        // A full or blocked storage must not break pairing for this session.
    }
}

/** Forget the token locally (revocation on the engine is separate). */
export function clearPairingToken (storage: TokenStorage | null = defaultStorage ()): void
{
    if (storage === null)
        return;

    try
    {
        storage.removeItem (PAIRING_TOKEN_KEY);
    }
    catch
    {
        // See savePairingToken().
    }
}

/** Keep only digits and cap the length at six. */
export function normalizePairingCode (input: string): string
{
    return input.replace (/\D/g, "").slice (0, 6);
}

/** True for exactly six decimal digits. */
export function isPairingCode (input: string): boolean
{
    return /^[0-9]{6}$/.test (input);
}

/**
    Read `?code=...` from a location search string.

    The QR code points at `/remote?code=123456`, so scanning it opens the
    remote already knowing the code.
*/
export function codeFromSearch (search: string): string | null
{
    const query = search.startsWith ("?") ? search.slice (1) : search;

    if (query.length === 0)
        return null;

    for (const part of query.split ("&"))
    {
        const separator = part.indexOf ("=");

        if (separator < 0)
            continue;

        const key = decodeURIComponent (part.slice (0, separator));

        if (key !== "code")
            continue;

        const value = normalizePairingCode (decodeURIComponent (part.slice (separator + 1)));

        return isPairingCode (value) ? value : null;
    }

    return null;
}

/** The URL shown in the QR code. */
export function pairingPayload (pairing: ControlPairingState | null): string
{
    return pairing?.payload ?? "";
}

/** Format the remaining code lifetime as "4:59". */
export function formatCodeLifetime (milliseconds: number): string
{
    if (!Number.isFinite (milliseconds) || milliseconds <= 0)
        return "expired";

    const totalSeconds = Math.ceil (milliseconds / 1000);
    const minutes = Math.floor (totalSeconds / 60);
    const seconds = totalSeconds % 60;

    return `${String (minutes)}:${String (seconds).padStart (2, "0")}`;
}

/** Split "123456" into "123 456" for the large code readout. */
export function spacedPairingCode (code: string): string
{
    if (code.length !== 6)
        return code;

    return `${code.slice (0, 3)} ${code.slice (3)}`;
}

/** True for the remote route; kept beside pairing because it consumes the code. */
export function isPairingRemotePath (pathname: string): boolean
{
    return pathname === "/remote" || pathname === "/remote/";
}
