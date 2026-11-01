import type { ControlPatchUpdate, ControlState } from "../api/client";

/**
    Return `state` with `slot` armed.

    Used for the optimistic highlight: the grid reads `armedSlot`, so patching
    this in the same tick as the click makes the highlight move on the next
    render instead of after the next 2 s poll.
*/
export function withArmedSlot (state: ControlState, slot: number): ControlState
{
    return {
        ...state,
        armedSlot: slot,
        slots: state.slots.map (candidate => ({
            ...candidate,
            armed: candidate.index === slot,
        })),
    };
}

/**
    Return `state` with a partial patch update applied to `slot`.

    Used for the same optimistic highlight rule as arming: a colour swatch
    moves the cell on the next render even if the engine round trip is slow.
*/
export function withPatchUpdate (state: ControlState,
                                 slot: number,
                                 patch: ControlPatchUpdate): ControlState
{
    return {
        ...state,
        families: state.families.map (family => ({
            ...family,
            patches: family.patches.map (candidate => candidate.slot === slot
                ? { ...candidate, ...patch }
                : candidate),
        })),
    };
}

/** Return `state` with one patch's `editorOpen` flag updated. */
export function withEditorOpen (state: ControlState,
                                slot: number,
                                open: boolean): ControlState
{
    return {
        ...state,
        families: state.families.map (family => ({
            ...family,
            patches: family.patches.map (candidate => candidate.slot === slot
                ? { ...candidate, editorOpen: open }
                : candidate),
        })),
    };
}
