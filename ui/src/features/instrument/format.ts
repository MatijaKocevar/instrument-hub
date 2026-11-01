/** Fader range, matching the API's `volumeDb` bounds. */
export const FADER_MIN_DB = -60;
export const FADER_MAX_DB = 12;
export const FADER_STEP_DB = 0.5;

/** Clamp a dB value into the fader's range. */
export function clampDb (db: number): number
{
    if (! Number.isFinite (db))
        return 0;

    return Math.min (FADER_MAX_DB, Math.max (FADER_MIN_DB, db));
}

/** Linear gain for a dB value; -60 dB is silence, exactly like the engine. */
export function gainFromDb (db: number): number
{
    const clamped = clampDb (db);

    return clamped <= FADER_MIN_DB ? 0 : 10 ** (clamped / 20);
}

/** Fixed-width dB readout; "-60.0 dB" is the engine's silence floor. */
export function formatDb (db: number): string
{
    return `${clampDb (db).toFixed (1)} dB`;
}

/** Clamp a balance value into -1 .. +1. */
export function clampPan (pan: number): number
{
    if (! Number.isFinite (pan))
        return 0;

    return Math.min (1, Math.max (-1, pan));
}

/** Balance readout: "L 40", "C", "R 40". */
export function formatPan (pan: number): string
{
    const clamped = clampPan (pan);

    if (Math.abs (clamped) < 0.005)
        return "C";

    return `${clamped < 0 ? "L" : "R"} ${String (Math.round (Math.abs (clamped) * 100))}`;
}

/** Wet amount as a whole percentage. */
export function wetPercent (wet: number): number
{
    return Math.round (Math.min (1, Math.max (0, Number.isFinite (wet) ? wet : 0)) * 100);
}

/** Compact byte count for the patch-state confirmation. */
export function formatStateSize (bytes: number): string
{
    if (! Number.isFinite (bytes) || bytes <= 0)
        return "0 B";

    if (bytes < 1024)
        return `${String (Math.round (bytes))} B`;

    if (bytes < 1024 * 1024)
        return `${(bytes / 1024).toFixed (1)} kB`;

    return `${(bytes / (1024 * 1024)).toFixed (1)} MB`;
}
