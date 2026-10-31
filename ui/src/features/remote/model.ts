import type { ControlState, ControlTransport } from "../../api/client";

/** Connection phases shared with the desktop store. */
export type RemoteConnection = "idle" | "connecting" | "ready" | "offline";

/** One playable entry in the remote's set list. */
export interface RemotePatch
{
    readonly id: string;
    readonly name: string;
    /** Fixed audio-graph slot index. */
    readonly slot: number;
    /** Instrument + FX chain latency in milliseconds. */
    readonly latencyMs: number;
}

/**
    Everything the stage remote renders.

    The remote is deliberately independent of the desktop grid: it only needs
    the ordered set (slots), the armed patch and the transport.
*/
export interface RemoteModel
{
    readonly applicationName: string;
    readonly patches: readonly RemotePatch[];
    /** Index into `patches`, or -1 when nothing is armed. */
    readonly armedSlot: number;
    readonly transport: ControlTransport;
}

export const EMPTY_REMOTE: RemoteModel = {
    applicationName: "Instrument Hub",
    patches: [],
    armedSlot: -1,
    transport: { playing: false, bpm: 120, ppq: 0, positionSamples: 0 },
};

/**
    Build the remote model from `GET /state`.

    A "patch" is one armed audio slot today. The set list keeps the engine's
    slot order so next/previous follows the performance order, not the
    alphabet.
*/
export function remoteFromState (state: ControlState | null): RemoteModel
{
    if (state === null)
        return EMPTY_REMOTE;

    const patches: RemotePatch[] = state.slots.map (slot => ({
        id: `slot-${String (slot.index)}`,
        name: slot.name,
        slot: slot.index,
        latencyMs: slot.latencyMs,
    }));

    return {
        applicationName: state.application.name,
        patches,
        armedSlot: Number.isInteger (state.armedSlot) ? state.armedSlot : -1,
        transport: state.transport,
    };
}

/** The armed patch, or `null` when the set is disarmed/empty. */
export function currentPatch (model: RemoteModel): RemotePatch | null
{
    return model.patches.find (patch => patch.slot === model.armedSlot) ?? null;
}

/**
    Return the slot `direction` steps from `current`, wrapping at both ends.

    With nothing armed, next starts at the first patch and previous at the
    last, so a single button always produces sound.
*/
export function stepSlot (current: number,
                          count: number,
                          direction: 1 | -1): number | null
{
    if (count <= 0)
        return null;

    if (current < 0)
        return direction > 0 ? 0 : count - 1;

    return (current + direction + count) % count;
}

/** Position of `slot` in the rendered set list, or -1. */
export function patchIndex (model: RemoteModel, slot: number): number
{
    return model.patches.findIndex (patch => patch.slot === slot);
}

/**
    The slot reached by moving `direction` entries through the set list.

    This walks the rendered set order, so a set with gaps in its slot numbers
    still moves from patch to patch without skipping to a missing slot.
*/
export function stepPatch (model: RemoteModel, direction: 1 | -1): number | null
{
    const count = model.patches.length;

    if (count === 0)
        return null;

    const current = patchIndex (model, model.armedSlot);
    const next = stepSlot (current, count, direction);

    return next === null ? null : model.patches[next]?.slot ?? null;
}


/** True for `/remote` and `/remote/` on any origin. */
export function isRemotePath (pathname: string): boolean
{
    return /\/remote\/?$/.test (pathname);
}

/** `128 BPM`, with one decimal only when the tempo is not an integer. */export function formatBpm (bpm: number): string
{
    const value = Number.isFinite (bpm) ? bpm : 0;

    return `${Number.isInteger (value) ? value.toFixed (0) : value.toFixed (1)} BPM`;
}

/** Musical position as `bar.beat`, assuming the engine's 4/4 grid. */
export function formatPosition (ppq: number): string
{
    const value = Number.isFinite (ppq) ? Math.max (0, ppq) : 0;
    const bar = Math.floor (value / 4) + 1;
    const beat = Math.floor (value % 4) + 1;

    return `${String (bar)}.${String (beat)}`;
}
