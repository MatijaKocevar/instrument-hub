import type { ControlPatchUpdate, ControlState } from "../api/client";

export function withArmedSlot(state: ControlState, slot: number): ControlState {
  return {
    ...state,
    armedSlot: slot,
    slots: state.slots.map((candidate) => ({
      ...candidate,
      armed: candidate.index === slot,
    })),
  };
}

export function withPatchUpdate(
  state: ControlState,
  slot: number,
  patch: ControlPatchUpdate,
): ControlState {
  return {
    ...state,
    families: state.families.map((family) => ({
      ...family,
      patches: family.patches.map((candidate) =>
        candidate.slot === slot ? { ...candidate, ...patch } : candidate,
      ),
    })),
  };
}

export function withEditorOpen(
  state: ControlState,
  slot: number,
  open: boolean,
): ControlState {
  return {
    ...state,
    families: state.families.map((family) => ({
      ...family,
      patches: family.patches.map((candidate) =>
        candidate.slot === slot
          ? { ...candidate, editorOpen: open }
          : candidate,
      ),
    })),
  };
}
