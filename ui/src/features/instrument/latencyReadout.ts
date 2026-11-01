/** Format a latency budget as "x.xx ms · N smp", clamped at zero. */
export function latencyReadout (samples: number, milliseconds: number): string
{
    return `${Math.max (0, milliseconds).toFixed (2)} ms · ${String (Math.max (0, samples))} smp`;
}
