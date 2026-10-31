import type { ControlState } from "../../api/client";

/** The grid is sized for the controller: 8 instrument families × 8 patches. */
export const GRID_ROWS = 8;
export const GRID_COLUMNS = 8;

/**
    The 16-colour patch palette.

    The palette index is stored in `/state` (`colorIndex`) and is shared with
    the Launchpad LED table: index N is the same patch colour on screen and on
    the controller. Index 0 is deliberately a rose rather than a pure red so
    the bright-red armed highlight is unambiguous.
*/
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

/** The armed cell is always bright red, independent of its palette index. */
export const ARMED_COLOUR = "#ff1f2f";

/** Clamp a palette index into 0..15. */
export function clampColourIndex (index: number): number
{
    if (! Number.isInteger (index))
        return 0;

    return Math.min (PATCH_COLOURS.length - 1, Math.max (0, index));
}

/** Hex colour for a palette index. */
export function colourForIndex (index: number): string
{
    return PATCH_COLOURS[clampColourIndex (index)] ?? PATCH_COLOURS[0];
}

/** The next palette index when a swatch is clicked. */
export function nextColourIndex (index: number): number
{
    return (clampColourIndex (index) + 1) % PATCH_COLOURS.length;
}

/** One playable patch inside a family. `slot` is a fixed audio-graph slot. */
export interface GridPatch
{
    readonly id: string;
    readonly name: string;
    /** Engine slot index, or `null` when the cell is defined but not playable yet. */
    readonly slot: number | null;
    /** Resolved palette index, 0 .. 15. */
    readonly colorIndex: number;
    /** Live warm-pool state, used by the controller LED mirror. */
    readonly warmState: string;
    /** Instrument + FX latency of the slot's chain, in samples. */
    readonly latencySamples?: number;
    /** The same chain latency in milliseconds at the engine sample rate. */
    readonly latencyMs?: number;
}

/** One family row: a named group of patches. */
export interface GridFamily
{
    readonly id: string;
    readonly name: string;
    readonly patches: readonly GridPatch[];
}

/** Everything the grid needs to render, independent of the API shape. */
export interface GridModel
{
    readonly families: readonly GridFamily[];
    /** Index of the armed slot, or -1. */
    readonly armedSlot: number;
}

/**
    Clamp a model to the grid's fixed 8×8 capacity.

    Extra families and patches are dropped rather than scrolled, which keeps a
    stray set file from changing the controller-facing layout.
*/
export function createGridModel (families: readonly GridFamily[],
                                 armedSlot: number): GridModel
{
    return {
        families: families.slice (0, GRID_ROWS).map (family => ({
            id: family.id,
            name: family.name,
            patches: family.patches.slice (0, GRID_COLUMNS).map (patch => ({
                ...patch,
                colorIndex: clampColourIndex (patch.colorIndex),
            })),
        })),
        armedSlot: Number.isInteger (armedSlot) ? armedSlot : -1,
    };
}

function slotsAsFamilies (state: ControlState): GridFamily[]
{
    const families: GridFamily[] = [];

    for (let offset = 0; offset < state.slots.length && families.length < GRID_ROWS;
         offset += GRID_COLUMNS)
    {
        const slots = state.slots.slice (offset, offset + GRID_COLUMNS);
        const first = slots[0];

        if (first === undefined)
            break;

        families.push ({
            id: `slot-family-${String (offset / GRID_COLUMNS)}`,
            name: slots.length === 1 ? first.name : `Slots ${String (offset + 1)}–${String (offset + slots.length)}`,
            patches: slots.map ((slot, column) => ({
                id: `slot-${String (slot.index)}`,
                name: slot.name,
                slot: slot.index,
                colorIndex: (offset + column) % PATCH_COLOURS.length,
                warmState: slot.armed ? "active" : "warm",
                latencySamples: slot.latencySamples,
                latencyMs: slot.latencyMs,
            })),
        });
    }

    return families;
}

/**
    Build the grid from `GET /state`.

    The control API publishes the real family tree (8 families × 8 patches), so
    the grid renders exactly what the Launchpad maps. Older state documents
    without `families` fall back to grouping the flat slot list into rows.
*/
export function gridFromState (state: ControlState | null): GridModel
{
    if (state === null)
        return { families: [], armedSlot: -1 };

    const families: GridFamily[] = state.families.length > 0
        ? state.families.map (family => ({
            id: family.id,
            name: family.name,
            patches: family.patches.map (patch => ({
                id: patch.id,
                name: patch.name,
                slot: patch.slot,
                colorIndex: patch.colorIndex,
                warmState: patch.warmState,
                latencySamples: patch.latencySamples,
                latencyMs: patch.latencyMs,
            })),
        }))
        : slotsAsFamilies (state);

    return createGridModel (families, state.armedSlot);
}

/** Every patch in the model, in family/patch order. */
export function gridPatches (model: GridModel): readonly GridPatch[]
{
    return model.families.flatMap (family => family.patches);
}

/** The patch hosted by `slot`, or null. */
export function patchForSlot (model: GridModel, slot: number | null): GridPatch | null
{
    if (slot === null || slot < 0)
        return null;

    return gridPatches (model).find (patch => patch.slot === slot) ?? null;
}
