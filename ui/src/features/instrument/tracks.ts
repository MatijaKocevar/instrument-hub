import type {
  ControlPatch,
  ControlState,
  ControlTrack,
} from "../../api/client";

export function trackForSlot(
  tracks: readonly ControlTrack[] | undefined,
  slot: number | null,
): ControlTrack | null {
  if (tracks === undefined || slot === null || slot < 0) return null;

  return tracks.find((track) => track.slot === slot) ?? null;
}

export function patchForTrack(
  state: ControlState,
  track: ControlTrack,
): ControlPatch {
  const familyPatch = state.families
    .flatMap((family) => family.patches)
    .find((patch) => patch.slot === track.slot);
  const slot = state.slots.find((candidate) => candidate.index === track.slot);

  return {
    slot: track.slot,
    id: track.id,
    name: track.name,
    family: track.id,
    familyName: track.name,
    instrument: track.instrument,
    volumeDb: familyPatch?.volumeDb ?? track.volumeDb,
    pan: familyPatch?.pan ?? 0,
    mute: familyPatch?.mute ?? false,
    solo: familyPatch?.solo ?? false,
    colorIndex: familyPatch?.colorIndex ?? track.index,
    warmState: familyPatch?.warmState ?? "warm",
    warm: familyPatch?.warm ?? true,
    editorOpen: familyPatch?.editorOpen ?? false,
    latencySamples: familyPatch?.latencySamples ?? slot?.latencySamples ?? 0,
    latencyMs: familyPatch?.latencyMs ?? slot?.latencyMs ?? 0,
  };
}

export function hotFxName(track: ControlTrack, hot: number): string {
  const entry = track.hotFx.find((candidate) => candidate.index === hot);

  if (entry === undefined || entry.id.length === 0) return "Unassigned";

  return entry.name.length > 0 ? entry.name : entry.id;
}
