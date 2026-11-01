import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type {
    ControlPatchEditor,
    ControlPatchSourceRequest,
    ControlPatchStateCapture,
    ControlPatchUpdate,
    ControlState,
    ControlTransport,
    ControlTransportPatch,
} from "./types";

/**
    State, arming, transport and patch editing.

    Every request answers with the state or the changed resource, so the store
    can reconcile its optimistic update from the same round trip.
*/
export interface StateEndpoints
{
    /** Read one consistent state snapshot. */
    getState (options?: { signal?: AbortSignal }): Promise<ControlState>;

    /**
        Arm `slot`, or disarm with `null` / a negative value.

        Resolves with the state after the request, so the caller can reconcile
        its optimistic highlight with the engine.
    */
    armSlot (slot: number | null, options?: { signal?: AbortSignal }): Promise<ControlState>;

    /** Start/stop or reposition the transport. */
    controlTransport (patch: ControlTransportPatch,
                      options?: { signal?: AbortSignal }): Promise<ControlTransport>;

    /**
        Change one patch's mix or colour.

        Resolves with the state after the request so the grid can reconcile its
        optimistic colour change with the engine.
    */
    updatePatch (slot: number,
                 patch: ControlPatchUpdate,
                 options?: { signal?: AbortSignal }): Promise<ControlState>;

    /**
        Change the instrument source behind one patch.

        The engine swaps the slot's source without rewiring the graph and
        answers with the state after the change, so the page can show the new
        descriptor from the same round trip.
    */
    changePatchSource (slot: number,
                       source: ControlPatchSourceRequest,
                       options?: { signal?: AbortSignal }): Promise<ControlState>;

    /**
        Open (or focus) one patch's native plugin editor.

        The engine opens the window on its message thread and answers with
        `{ slot, open, reused }`. A generated or sampler source, or a plugin
        that cannot create an editor, is rejected with a readable error.
    */
    openPatchEditor (slot: number,
                     options?: { signal?: AbortSignal }): Promise<ControlPatchEditor>;

    /** Close one patch's native editor. Idempotent. */
    closePatchEditor (slot: number,
                      options?: { signal?: AbortSignal }): Promise<ControlPatchEditor>;

    /**
        Save the running plugin's current sound into its patch.

        Resolves with `{ slot, captured, sizeBytes }` after the engine has
        written the patch's state blob, so the page can confirm the save
        without switching patches.
    */
    capturePatchState (slot: number,
                       options?: { signal?: AbortSignal }): Promise<ControlPatchStateCapture>;
}

/** Rejects payloads the UI cannot render (wrong version, missing arrays). */
function isControlState (value: unknown): value is ControlState
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlState>;

    return candidate.schemaVersion === 1
        && typeof candidate.application?.name === "string"
        && typeof candidate.application?.version === "string"
        && typeof candidate.armedSlot === "number"
        && Array.isArray (candidate.slots)
        && candidate.slots.every (slot =>
               slot !== null
               && typeof slot === "object"
               && typeof slot.index === "number"
               && typeof slot.name === "string"
               && typeof slot.armed === "boolean"
               && typeof slot.latencySamples === "number"
               && typeof slot.latencyMs === "number")
        && Array.isArray (candidate.families)
        && candidate.families.every (family =>
               family !== null
               && typeof family === "object"
               && typeof family.id === "string"
               && typeof family.name === "string"
               && Array.isArray (family.patches)
               && family.patches.every (patch =>
                      patch !== null
                      && typeof patch === "object"
                      && typeof patch.slot === "number"
                      && typeof patch.name === "string"
                      && typeof patch.colorIndex === "number"))
        && typeof candidate.settings?.masterGain === "number"
        && typeof candidate.transport?.bpm === "number";
}

/** Rejects transport payloads the remote cannot render. */
function isTransport (value: unknown): value is ControlTransport
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlTransport>;

    return typeof candidate.playing === "boolean"
        && typeof candidate.bpm === "number"
        && typeof candidate.ppq === "number"
        && typeof candidate.positionSamples === "number";
}

function requireState (guard: RequestGuard,
                       data: ControlState | undefined,
                       error: unknown,
                       status: number,
                       what: string): ControlState
{
    if (data !== undefined)
    {
        if (! isControlState (data))
            return guard.reject (`${what} returned an unsupported payload`, status);

        return data;
    }

    return guard.fail (error, status, what);
}

function requireTransport (guard: RequestGuard,
                           data: ControlTransport | undefined,
                           error: unknown,
                           status: number,
                           what: string): ControlTransport
{
    if (data !== undefined && isTransport (data))
        return data;

    return guard.fail (error, status, what);
}

function requireEditor (guard: RequestGuard,
                        data: ControlPatchEditor | undefined,
                        error: unknown,
                        status: number,
                        what: string): ControlPatchEditor
{
    if (data !== undefined
        && typeof data.slot === "number"
        && typeof data.open === "boolean")
        return data;

    return guard.fail (error, status, what);
}

function requireStateCapture (guard: RequestGuard,
                              data: ControlPatchStateCapture | undefined,
                              error: unknown,
                              status: number,
                              what: string): ControlPatchStateCapture
{
    if (data !== undefined
        && typeof data.slot === "number"
        && typeof data.captured === "boolean"
        && typeof data.sizeBytes === "number")
        return data;

    return guard.fail (error, status, what);
}

export function createStateEndpoints (client: Client<paths>,
                                      guard: RequestGuard): StateEndpoints
{
    return {
        async getState (requestOptions)
        {
            const { data, error, response } = await client.GET ("/state", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (guard, data, error, response.status, "GET /state");
        },

        async armSlot (slot, requestOptions)
        {
            const { data, error, response } = await client.POST ("/arm", {
                body: { slot },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (guard, data, error, response.status, "POST /arm");
        },

        async controlTransport (patch, requestOptions)
        {
            const { data, error, response } = await client.POST ("/transport", {
                body: patch,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireTransport (guard, data, error, response.status, "POST /transport");
        },

        async updatePatch (slot, patch, requestOptions)
        {
            const { data, error, response } = await client.PATCH ("/patches/{slot}", {
                params: { path: { slot } },
                body: patch,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (guard, data, error, response.status,
                                 `PATCH /patches/${String (slot)}`);
        },

        async changePatchSource (slot, source, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/source", {
                params: { path: { slot } },
                body: source,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (guard, data, error, response.status,
                                 `POST /patches/${String (slot)}/source`);
        },

        async openPatchEditor (slot, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/editor", {
                params: { path: { slot } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireEditor (guard, data, error, response.status,
                                  `POST /patches/${String (slot)}/editor`);
        },

        async closePatchEditor (slot, requestOptions)
        {
            const { data, error, response } = await client.DELETE ("/patches/{slot}/editor", {
                params: { path: { slot } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireEditor (guard, data, error, response.status,
                                  `DELETE /patches/${String (slot)}/editor`);
        },

        async capturePatchState (slot, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/state/capture", {
                params: { path: { slot } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireStateCapture (guard, data, error, response.status,
                                        `POST /patches/${String (slot)}/state/capture`);
        },
    };
}
