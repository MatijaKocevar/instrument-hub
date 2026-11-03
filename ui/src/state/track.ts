import type {
  ControlState,
  ControlTrack,
  ControlTrackHotFx,
} from "../api/client";
import {
  clampTrackVolumeStep,
  trackDbForStep,
  trackGainForStep,
} from "../features/grid";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

function trackAt(
  state: ControlState | null,
  index: number,
): ControlTrack | null {
  return state?.tracks?.find((track) => track.index === index) ?? null;
}

function withSelectedTrack(state: ControlState, index: number): ControlState {
  const track = trackAt(state, index);

  if (track === null) return state;

  return {
    ...state,
    armedSlot: track.slot,
    slots: state.slots.map((slot) => ({
      ...slot,
      armed: slot.index === track.slot,
    })),
    tracks: (state.tracks ?? []).map((candidate) => ({
      ...candidate,
      selected: candidate.index === index,
    })),
  };
}

function withToggledHotFx(
  state: ControlState,
  index: number,
  hot: number,
): ControlState {
  return {
    ...state,
    tracks: (state.tracks ?? []).map((track) =>
      track.index !== index
        ? track
        : {
            ...track,
            hotFx: track.hotFx.map((entry) =>
              entry.index === hot
                ? { ...entry, bypassed: !entry.bypassed }
                : entry,
            ),
          },
    ),
  };
}

function withTrackVolume(
  state: ControlState,
  index: number,
  step: number,
): ControlState {
  const clamped = clampTrackVolumeStep(step);

  return {
    ...state,
    tracks: (state.tracks ?? []).map((track) =>
      track.index !== index
        ? track
        : {
            ...track,
            volumeStep: clamped,
            volumeDb: trackDbForStep(clamped),
            gain: trackGainForStep(clamped),
          },
    ),
  };
}

function withHotFxAssignment(
  state: ControlState,
  index: number,
  hot: number,
  insertId: string,
  name: string,
  bypassed: boolean,
): ControlState {
  return {
    ...state,
    tracks: (state.tracks ?? []).map((track) => {
      if (track.index !== index) return track;

      const cleared = insertId.length
        ? track.hotFx.map((entry): ControlTrackHotFx =>
            entry.index !== hot && entry.id === insertId
              ? { ...entry, id: "", name: "", bypassed: false }
              : entry,
          )
        : track.hotFx;

      return {
        ...track,
        hotFx: cleared.map((entry) =>
          entry.index === hot
            ? { ...entry, id: insertId, name, bypassed }
            : entry,
        ),
      };
    }),
  };
}

export class TrackActions {
  private sequence = 0;
  private inFlight = 0;

  constructor(private readonly context: ActionContext) {}

  selectTrack = async (index: number): Promise<boolean> => {
    const previous = this.snapshot.state;

    if (trackAt(previous, index) === null) return false;

    return this.run(
      (state) => withSelectedTrack(state, index),
      () => this.context.api.selectTrack(index),
    );
  };

  toggleTrackFx = async (index: number, hot: number): Promise<boolean> => {
    const track = trackAt(this.snapshot.state, index);
    const assigned =
      track?.hotFx.some(
        (entry) => entry.index === hot && entry.id.length > 0,
      ) ?? false;

    if (!assigned) return false;

    return this.run(
      (state) => withToggledHotFx(state, index, hot),
      () => this.context.api.toggleTrackFx(index, hot),
    );
  };

  setTrackVolume = async (index: number, step: number): Promise<boolean> => {
    if (!Number.isInteger(step) || step < 0 || step > 5) return false;
    if (trackAt(this.snapshot.state, index) === null) return false;

    return this.run(
      (state) => withTrackVolume(state, index, step),
      () => this.context.api.setTrackVolume(index, step),
    );
  };

  assignTrackHotFx = async (
    index: number,
    hot: number,
    insertId: string | null,
  ): Promise<boolean> => {
    const track = trackAt(this.snapshot.state, index);

    if (track === null || hot < 0 || hot > 1) return false;

    const id = insertId ?? "";
    const chain =
      this.snapshot.fxSlot === track.slot ? this.snapshot.fxChain : null;
    const insert = chain?.inserts.find((candidate) => candidate.id === id);
    const name = insert?.name ?? "";

    return this.run(
      (state) =>
        withHotFxAssignment(
          state,
          index,
          hot,
          id,
          name,
          insert?.bypassed ?? false,
        ),
      () => this.context.api.assignTrackFx(index, hot, id),
    );
  };

  preservePending = (state: ControlState): ControlState => {
    const current = this.snapshot.state;

    if (this.inFlight === 0 || current === null) return state;

    return {
      ...state,
      armedSlot: current.armedSlot,
      slots: state.slots.map((slot) => ({
        ...slot,
        armed: slot.index === current.armedSlot,
      })),
      ...(current.tracks !== undefined ? { tracks: current.tracks } : {}),
    };
  };

  private async run(
    optimistic: (state: ControlState) => ControlState,
    request: () => Promise<ControlState>,
  ): Promise<boolean> {
    const previous = this.snapshot.state;

    if (previous === null) return false;

    const sequence = this.sequence + 1;
    this.sequence = sequence;
    this.inFlight += 1;

    this.patch({ state: optimistic(previous), error: null });

    try {
      const state = await request();

      if (sequence === this.sequence)
        this.patch({
          phase: "ready",
          state,
          error: null,
          failedAttempts: 0,
          updatedAt: this.context.now(),
        });

      return true;
    } catch (error) {
      if (sequence === this.sequence)
        this.patch({ error: messageOf(error), state: previous });

      return false;
    } finally {
      this.inFlight -= 1;
    }
  }

  private get snapshot() {
    return this.context.getSnapshot();
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
