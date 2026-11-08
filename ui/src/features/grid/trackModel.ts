import type {
  ControlPatchInstrument,
  ControlTrackHotFx,
} from "../../api/client";
import { clampColourIndex } from "./colours";
import type { GridModel, GridPageStatus } from "./model";
import {
  clampTrackVolumeGain,
  trackDbForGain,
  trackGainForStep,
  trackVolumeStepForGain,
} from "./trackVolume";

export interface GridHotFx {
  readonly index: number;
  readonly id: string;
  readonly name: string;
  readonly bypassed: boolean;
}

export type TrackStatus = "ok" | "warming" | "error" | "empty";

export interface GridTrack {
  readonly index: number;
  readonly id: string;
  readonly name: string;
  readonly slot: number;
  readonly selected: boolean;
  readonly colorIndex: number;
  readonly status: TrackStatus;
  readonly hasInstrument: boolean;
  readonly instrument: ControlPatchInstrument;
  readonly volumeStep: number;
  readonly volumeDb: number;
  readonly gain: number;
  readonly hotFx: readonly GridHotFx[];
}

export type TrackGridRole = "track" | "fx" | "volume";

export interface TrackGridRowSpec {
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
  readonly warmState: string;
  readonly error: string;
  readonly instrument: ControlPatchInstrument;
  readonly volumeStep: number;
  readonly volumeDb: number;
  readonly gain: number;
  readonly hotFx: readonly ControlTrackHotFx[];
}

export const TRACK_GRID_ROWS: readonly TrackGridRowSpec[] = [
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

function trackStatus(
  track: TrackGridTrackInput,
  page: GridPageStatus | null,
): TrackStatus {
  if (track.instrument.kind === "empty") return "empty";
  if (track.error.length > 0) return "error";

  const onPage = page?.slots.includes(track.slot) === true;

  if (onPage && page?.status === "failed") return "error";

  if (
    track.warmState === "cold" ||
    track.warmState === "warming" ||
    track.warmState === "fading" ||
    (onPage && page?.status === "recovering")
  )
    return "warming";

  return "ok";
}

export function createTrackGridModel(
  tracks: readonly TrackGridTrackInput[],
  armedSlot: number,
  pluginPage: GridPageStatus | null = null,
): GridModel {
  return {
    layout: "tracks",
    pluginPage,
    tracks: tracks.map((track) => {
      const gain = clampTrackVolumeGain(
        Number.isFinite(track.gain) && track.gain >= 0
          ? track.gain
          : trackGainForStep(track.volumeStep),
      );

      return {
        index: track.index,
        id: track.id,
        name: track.name,
        slot: track.slot,
        selected: track.selected,
        colorIndex: clampColourIndex(track.index),
        status: trackStatus(track, pluginPage),
        hasInstrument: track.instrument.kind !== "empty",
        instrument: track.instrument,
        volumeStep: trackVolumeStepForGain(gain),
        volumeDb: trackDbForGain(gain),
        gain,
        hotFx: [hotForIndex(track.hotFx, 0), hotForIndex(track.hotFx, 1)],
      };
    }),
    armedSlot: Number.isInteger(armedSlot) ? armedSlot : -1,
  };
}
