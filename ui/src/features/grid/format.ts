/** Two-digit zero padding for the grid's fixed-width slot and column labels. */
export function pad2 (value: number): string
{
    return value.toString ().padStart (2, "0");
}

/** A compact, fixed-width chain latency readout for a patch cell. */
export function formatLatency (milliseconds: number): string
{
    const clamped = Math.max (0, Number.isFinite (milliseconds) ? milliseconds : 0);

    return `${clamped.toFixed (2)} ms`;
}

/** Map a linear peak (0..1) to the meter fill width in percent. */
export function meterPercent (peak: number): number
{
    const clamped = Math.min (1, Math.max (0, Number.isFinite (peak) ? peak : 0));

    // A square-root scale gives quiet material visible movement without a
    // logarithmic lookup table in the render path.
    return Math.round (Math.sqrt (clamped) * 100);
}
