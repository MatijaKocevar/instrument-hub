const MAX_COLOUR_INDEX = 15;

export const EMPTY_COLOUR = "#64748b";

export function clampColourIndex(index: number): number {
  if (!Number.isInteger(index)) return 0;

  return Math.min(MAX_COLOUR_INDEX, Math.max(0, index));
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
