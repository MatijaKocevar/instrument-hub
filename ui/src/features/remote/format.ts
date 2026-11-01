export function pad2(value: number): string {
  return value.toString().padStart(2, "0");
}

export function formatLatency(milliseconds: number): string {
  const clamped = Math.max(0, Number.isFinite(milliseconds) ? milliseconds : 0);

  return `${clamped.toFixed(1)} ms`;
}
