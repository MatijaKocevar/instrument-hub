import type { ControlState } from "../../api/client";
import {
  createTrackGridModel,
  type GridTrack,
  type TrackGridTrackInput,
} from "./trackModel";

const MAX_TRACKS = 8;

export type GridLayout = "tracks";

export interface GridPageStatus {
  readonly id: string;
  readonly status: string;
  readonly slots: readonly number[];
  readonly activeSlot: number;
  readonly error: string;
}

export interface GridModel {
  readonly layout: GridLayout;
  readonly tracks: readonly GridTrack[];
  readonly armedSlot: number;
  readonly pluginPage: GridPageStatus | null;
}

function trackInputs(state: ControlState): TrackGridTrackInput[] {
  const slots = new Map(state.slots.map((slot) => [slot.index, slot]));
  const tracks = state.tracks ?? [];

  return [...tracks]
    .sort((left, right) => left.index - right.index)
    .slice(0, MAX_TRACKS)
    .map((track) => {
      const slot = slots.get(track.slot);

      return {
        index: track.index,
        id: track.id,
        name: track.name,
        slot: track.slot,
        selected: track.selected || track.slot === state.armedSlot,
        colorIndex: track.index,
        warmState: slot?.warmState ?? "cold",
        error: slot?.error ?? "",
        instrument: track.instrument,
        volumeStep: track.volumeStep,
        volumeDb: track.volumeDb,
        gain: track.gain,
        hotFx: track.hotFx,
      };
    });
}

function pageStatusFor(state: ControlState): GridPageStatus | null {
  const pages = state.pages ?? [];
  const page =
    pages.find((candidate) => candidate.slots.includes(state.armedSlot)) ??
    pages[0];

  if (page === undefined) return null;

  return {
    id: page.id,
    status: page.status,
    slots: page.slots,
    activeSlot: page.activeSlot,
    error: page.error,
  };
}

export function gridFromState(state: ControlState | null): GridModel {
  if (state === null)
    return {
      layout: "tracks",
      tracks: [],
      armedSlot: -1,
      pluginPage: null,
    };

  return createTrackGridModel(
    trackInputs(state),
    state.armedSlot,
    pageStatusFor(state),
  );
}
