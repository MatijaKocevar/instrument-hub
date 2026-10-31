import type { ControlState } from "../../api/client";

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
            patches: family.patches.slice (0, GRID_COLUMNS),
        })),
        armedSlot: Number.isInteger (armedSlot) ? armedSlot : -1,
    };
}

/**
    Build the grid from `GET /state`.

    The control API currently publishes flat slots, not the set's family tree,
    so each slot is shown as a one-patch family. The grid itself is family- and
    patch-shaped, so a future `/state` field can replace this adapter without
    touching the view.
*/
export function gridFromState (state: ControlState | null): GridModel
{
    if (state === null)
        return { families: [], armedSlot: -1 };

    const families: GridFamily[] = state.slots.slice (0, GRID_ROWS).map (slot => ({
        id: `slot-family-${String (slot.index)}`,
        name: slot.name,
        patches: [
            {
                id: `slot-${String (slot.index)}`,
                name: slot.name,
                slot: slot.index,
            },
        ],
    }));

    return createGridModel (families, state.armedSlot);
}
