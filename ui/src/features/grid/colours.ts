export const PATCH_COLOURS = [
  "#eab308",
  "#22c55e",
  "#14b8a6",
  "#06b6d4",
  "#3b82f6",
  "#8b5cf6",
  "#e2e8f0",
  "#94a3b8",
  "#a3e635",
  "#34d399",
  "#38bdf8",
  "#818cf8",
  "#a78bfa",
  "#fde047",
  "#64748b",
  "#d6d3d1",
] as const;

export const ARMED_COLOUR = "#ff1f2f";

export const EMPTY_COLOUR = "#64748b";

export function clampColourIndex(index: number): number {
  if (!Number.isInteger(index)) return 0;

  return Math.min(PATCH_COLOURS.length - 1, Math.max(0, index));
}

export function colourForIndex(index: number): string {
  return PATCH_COLOURS[clampColourIndex(index)] ?? PATCH_COLOURS[0];
}

export function nextColourIndex(index: number): number {
  return (clampColourIndex(index) + 1) % PATCH_COLOURS.length;
}

export const TRACK_COLOURS = [
  "#ff6b00",
  "#6a00ff",
  "#ff2d6f",
  "#00a88f",
  "#ff00c8",
  "#a16207",
  "#e5e7eb",
  "#7e22ce",
] as const;

export function trackColourForIndex(index: number): string {
  if (!Number.isInteger(index)) return TRACK_COLOURS[0];

  return (
    TRACK_COLOURS[
      ((index % TRACK_COLOURS.length) + TRACK_COLOURS.length) %
        TRACK_COLOURS.length
    ] ?? TRACK_COLOURS[0]
  );
}
