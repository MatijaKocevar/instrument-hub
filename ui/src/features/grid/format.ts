export function pad2 (value: number): string
{
    return value.toString ().padStart (2, "0");
}

export function formatLatency (milliseconds: number): string
{
    const clamped = Math.max (0, Number.isFinite (milliseconds) ? milliseconds : 0);

    return `${clamped.toFixed (2)} ms`;
}

export function meterPercent (peak: number): number
{
    const clamped = Math.min (1, Math.max (0, Number.isFinite (peak) ? peak : 0));

    return Math.round (Math.sqrt (clamped) * 100);
}
