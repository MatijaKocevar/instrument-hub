/**
    Stage contrast modes.

    The remote defaults to `high`: pure black behind pure white text with a
    saturated accent, so it stays readable under stage lights and through a
    tablet's screen protector. `standard` keeps the softer desktop palette for
    a dim room.
*/
export type RemoteContrast = "high" | "standard";

export const DEFAULT_REMOTE_CONTRAST: RemoteContrast = "high";

/** localStorage key for the chosen stage contrast. */
export const REMOTE_CONTRAST_KEY = "instrument-hub.remote.contrast";

/** The minimum surface of `localStorage` this feature needs. */
export interface ContrastStorage
{
    getItem (key: string): string | null;
    setItem (key: string, value: string): void;
}

export function isRemoteContrast (value: unknown): value is RemoteContrast
{
    return value === "high" || value === "standard";
}

/** The mode reached by flipping the toggle. */
export function nextContrast (current: RemoteContrast): RemoteContrast
{
    return current === "high" ? "standard" : "high";
}

/** Readable label for the contrast control. */
export function contrastLabel (current: RemoteContrast): string
{
    return current === "high" ? "High contrast" : "Standard";
}

/** Read the stored mode; missing or unreadable storage means high contrast. */
export function loadRemoteContrast (storage: ContrastStorage | null): RemoteContrast
{
    if (storage === null)
        return DEFAULT_REMOTE_CONTRAST;

    try
    {
        const stored = storage.getItem (REMOTE_CONTRAST_KEY);

        return isRemoteContrast (stored) ? stored : DEFAULT_REMOTE_CONTRAST;
    }
    catch
    {
        return DEFAULT_REMOTE_CONTRAST;
    }
}

/** Persist the choice; a blocked store must not break the stage. */
export function saveRemoteContrast (storage: ContrastStorage | null,
                                    contrast: RemoteContrast): void
{
    if (storage === null)
        return;

    try
    {
        storage.setItem (REMOTE_CONTRAST_KEY, contrast);
    }
    catch
    {
        // Private mode or a full quota: keep the in-memory choice.
    }
}
