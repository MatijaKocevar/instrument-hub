import type { ControlState } from "../../api/client";
import { clampColourIndex, PATCH_COLOURS } from "./colours";
import {
  createTrackGridModel,
  type GridTrack,
  type TrackGridTrackInput,
} from "./trackModel";

export const GRID_ROWS = 8;
export const GRID_COLUMNS = 8;

export type GridLayout = "families" | "tracks";

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
  readonly layout: GridLayout;
  readonly families: readonly GridFamily[];
  readonly tracks: readonly GridTrack[];
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
    layout: "families",
    families: families.slice(0, GRID_ROWS).map((family) => ({
      id: family.id,
      name: family.name,
      patches: family.patches.slice(0, GRID_COLUMNS).map((patch) => ({
        ...patch,
        colorIndex: clampColourIndex(patch.colorIndex),
      })),
    })),
    tracks: [],
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

function colourIndexBySlot(state: ControlState): Map<number, number> {
  const colours = new Map<number, number>();

  for (const family of state.families)
    for (const patch of family.patches)
      colours.set(patch.slot, clampColourIndex(patch.colorIndex));

  return colours;
}

function trackInputs(state: ControlState): TrackGridTrackInput[] {
  const colours = colourIndexBySlot(state);
  const tracks = state.tracks ?? [];

  return [...tracks]
    .sort((left, right) => left.index - right.index)
    .slice(0, GRID_COLUMNS)
    .map((track) => ({
      index: track.index,
      id: track.id,
      name: track.name,
      slot: track.slot,
      selected: track.selected || track.slot === state.armedSlot,
      colorIndex: colours.get(track.slot) ?? track.index,
      instrument: track.instrument,
      volumeStep: track.volumeStep,
      volumeDb: track.volumeDb,
      gain: track.gain,
      hotFx: track.hotFx,
    }));
}

export function gridFromState(state: ControlState | null): GridModel {
  if (state === null)
    return { layout: "families", families: [], tracks: [], armedSlot: -1 };

  if (state.layout === "tracks" && (state.tracks?.length ?? 0) > 0)
    return createTrackGridModel(trackInputs(state), state.armedSlot);

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
