import type { ControlState } from "../../api/client";
import { clampColourIndex, PATCH_COLOURS } from "./colours";

export const GRID_ROWS = 8;
export const GRID_COLUMNS = 8;

export interface GridPatch {
  readonly id: string;
  readonly name: string;
  readonly slot: number | null;
  readonly colorIndex: number;
  readonly warmState: string;
  readonly latencySamples?: number;
  readonly latencyMs?: number;
}

export interface GridFamily {
  readonly id: string;
  readonly name: string;
  readonly patches: readonly GridPatch[];
}

export interface GridModel {
  readonly families: readonly GridFamily[];
  readonly armedSlot: number;
}

export interface GridRow {
  readonly family: GridFamily;
  readonly row: number;
}

export function createGridModel(
  families: readonly GridFamily[],
  armedSlot: number,
): GridModel {
  return {
    families: families.slice(0, GRID_ROWS).map((family) => ({
      id: family.id,
      name: family.name,
      patches: family.patches.slice(0, GRID_COLUMNS).map((patch) => ({
        ...patch,
        colorIndex: clampColourIndex(patch.colorIndex),
      })),
    })),
    armedSlot: Number.isInteger(armedSlot) ? armedSlot : -1,
  };
}

function slotsAsFamilies(state: ControlState): GridFamily[] {
  const families: GridFamily[] = [];

  for (
    let offset = 0;
    offset < state.slots.length && families.length < GRID_ROWS;
    offset += GRID_COLUMNS
  ) {
    const slots = state.slots.slice(offset, offset + GRID_COLUMNS);
    const first = slots[0];

    if (first === undefined) break;

    families.push({
      id: `slot-family-${String(offset / GRID_COLUMNS)}`,
      name:
        slots.length === 1
          ? first.name
          : `Slots ${String(offset + 1)}–${String(offset + slots.length)}`,
      patches: slots.map((slot, column) => ({
        id: `slot-${String(slot.index)}`,
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

export function gridFromState(state: ControlState | null): GridModel {
  if (state === null) return { families: [], armedSlot: -1 };

  const families: GridFamily[] =
    state.families.length > 0
      ? state.families.map((family) => ({
          id: family.id,
          name: family.name,
          patches: family.patches.map((patch) => ({
            id: patch.id,
            name: patch.name,
            slot: patch.slot,
            colorIndex: patch.colorIndex,
            warmState: patch.warmState,
            latencySamples: patch.latencySamples,
            latencyMs: patch.latencyMs,
          })),
        }))
      : slotsAsFamilies(state);

  return createGridModel(families, state.armedSlot);
}

export function gridRows(model: GridModel): readonly GridRow[] {
  return model.families.map((family, row) => ({ family, row })).reverse();
}

export function gridPatches(model: GridModel): readonly GridPatch[] {
  return model.families.flatMap((family) => family.patches);
}

export function patchForSlot(
  model: GridModel,
  slot: number | null,
): GridPatch | null {
  if (slot === null || slot < 0) return null;

  return gridPatches(model).find((patch) => patch.slot === slot) ?? null;
}
