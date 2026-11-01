export const PATCH_COLOURS = [
    "#f43f5e",
    "#fb923c",
    "#facc15",
    "#a3e635",
    "#34d399",
    "#2dd4bf",
    "#22d3ee",
    "#38bdf8",
    "#818cf8",
    "#a78bfa",
    "#c084fc",
    "#e879f9",
    "#f472b6",
    "#fb7185",
    "#94a3b8",
    "#e2e8f0",
] as const;

export const ARMED_COLOUR = "#ff1f2f";

export function clampColourIndex (index: number): number
{
    if (! Number.isInteger (index))
        return 0;

    return Math.min (PATCH_COLOURS.length - 1, Math.max (0, index));
}

export function colourForIndex (index: number): string
{
    return PATCH_COLOURS[clampColourIndex (index)] ?? PATCH_COLOURS[0];
}

export function nextColourIndex (index: number): number
{
    return (clampColourIndex (index) + 1) % PATCH_COLOURS.length;
}
