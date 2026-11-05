import type {
  ControlPatchInstrument,
  ControlTrackHotFx,
} from "../../api/client";
import { clampColourIndex } from "./colours";
import type { GridModel, GridPageStatus } from "./model";
import {
  clampTrackVolumeStep,
  trackDbForStep,
  trackGainForStep,
} from "./trackVolume";

export interface GridHotFx {
  readonly index: number;
  readonly id: string;
  readonly name: string;
  readonly bypassed: boolean;
}

export interface GridTrack {
  readonly index: number;
  readonly id: string;
  readonly name: string;
  readonly slot: number;
  readonly selected: boolean;
  readonly colorIndex: number;
  readonly hasInstrument: boolean;
  readonly instrument: ControlPatchInstrument;
  readonly volumeStep: number;
  readonly volumeDb: number;
  readonly gain: number;
  readonly hotFx: readonly GridHotFx[];
}

export type TrackGridRole = "track" | "fx" | "volume";

export interface TrackGridRow {
  readonly row: number;
  readonly role: TrackGridRole;
  readonly hot?: number;
  readonly step?: number;
  readonly label: string;
}

export interface TrackGridTrackInput {
  readonly index: number;
  readonly id: string;
  readonly name: string;
  readonly slot: number;
  readonly selected: boolean;
  readonly colorIndex: number;
  readonly instrument: ControlPatchInstrument;
  readonly volumeStep: number;
  readonly volumeDb: number;
  readonly gain: number;
  readonly hotFx: readonly ControlTrackHotFx[];
}

export const TRACK_GRID_ROWS: readonly TrackGridRow[] = [
  { row: 7, role: "track", label: "Track" },
  { row: 6, role: "fx", hot: 0, label: "FX 1" },
  { row: 5, role: "fx", hot: 1, label: "FX 2" },
  { row: 4, role: "volume", step: 4, label: "+3 dB" },
  { row: 3, role: "volume", step: 3, label: "100%" },
  { row: 2, role: "volume", step: 2, label: "75%" },
  { row: 1, role: "volume", step: 1, label: "50%" },
  { row: 0, role: "volume", step: 0, label: "0%" },
];

function hotForIndex(
  entries: readonly ControlTrackHotFx[],
  index: number,
): GridHotFx {
  const entry = entries.find((candidate) => candidate.index === index);

  return {
    index,
    id: entry?.id ?? "",
    name: entry?.name ?? "",
    bypassed: entry?.bypassed ?? false,
  };
}

export function createTrackGridModel(
  tracks: readonly TrackGridTrackInput[],
  armedSlot: number,
  pluginPage: GridPageStatus | null = null,
): GridModel {
  return {
    layout: "tracks",
    families: [],
    pluginPage,
    tracks: tracks.map((track) => {
      const volumeStep = clampTrackVolumeStep(track.volumeStep);

      return {
        index: track.index,
        id: track.id,
        name: track.name,
        slot: track.slot,
        selected: track.selected,
        colorIndex: clampColourIndex(track.index),
        hasInstrument: track.instrument.kind !== "empty",
        instrument: track.instrument,
        volumeStep,
        volumeDb:
          Number.isFinite(track.volumeDb) && track.volumeDb !== 0
            ? track.volumeDb
            : trackDbForStep(volumeStep),
        gain:
          Number.isFinite(track.gain) && track.gain > 0
            ? track.gain
            : trackGainForStep(volumeStep),
        hotFx: [hotForIndex(track.hotFx, 0), hotForIndex(track.hotFx, 1)],
      };
    }),
    armedSlot: Number.isInteger(armedSlot) ? armedSlot : -1,
  };
}
