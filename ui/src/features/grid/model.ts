import type { ControlState } from "../../api/client";
import { clampColourIndex, PATCH_COLOURS } from "./colours";

/** The grid is sized for the controller: 8 instrument families × 8 patches. */
export const GRID_ROWS = 8;
export const GRID_COLUMNS = 8;

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
