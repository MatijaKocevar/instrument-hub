import type { ControlState, ControlTransport } from "../../api/client";

export type RemoteConnection = "idle" | "connecting" | "ready" | "offline";

export interface RemotePatch
{
    readonly id: string;
    readonly name: string;
    readonly slot: number;
    readonly latencyMs: number;
}

export interface RemoteModel
{
    readonly applicationName: string;
    readonly patches: readonly RemotePatch[];
    readonly armedSlot: number;
    readonly transport: ControlTransport;
}

export const EMPTY_REMOTE: RemoteModel = {
    applicationName: "Instrument Hub",
    patches: [],
    armedSlot: -1,
    transport: { playing: false, bpm: 120, ppq: 0, positionSamples: 0 },
};

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

export function currentPatch (model: RemoteModel): RemotePatch | null
{
    return model.patches.find (patch => patch.slot === model.armedSlot) ?? null;
}

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

export function patchIndex (model: RemoteModel, slot: number): number
{
    return model.patches.findIndex (patch => patch.slot === slot);
}

export function stepPatch (model: RemoteModel, direction: 1 | -1): number | null
{
    const count = model.patches.length;

    if (count === 0)
        return null;

    const current = patchIndex (model, model.armedSlot);
    const next = stepSlot (current, count, direction);

    return next === null ? null : model.patches[next]?.slot ?? null;
}


export function isRemotePath (pathname: string): boolean
{
    return /\/remote\/?$/.test (pathname);
}

export function formatBpm (bpm: number): string
{
    const value = Number.isFinite (bpm) ? bpm : 0;

    return `${Number.isInteger (value) ? value.toFixed (0) : value.toFixed (1)} BPM`;
}

export function formatPosition (ppq: number): string
{
    const value = Number.isFinite (ppq) ? Math.max (0, ppq) : 0;
    const bar = Math.floor (value / 4) + 1;
    const beat = Math.floor (value % 4) + 1;

    return `${String (bar)}.${String (beat)}`;
}
