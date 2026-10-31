import createClient from "openapi-fetch";

import type { components, paths } from "./schema";

/** The complete control state returned by `GET /state`. */
export type ControlState = components["schemas"]["State"];
export type ControlSlot = components["schemas"]["Slot"];
export type ControlSettings = components["schemas"]["Settings"];
export type ControlTransport = components["schemas"]["Transport"];
export type ControlTransportPatch = components["schemas"]["TransportPatch"];
export type ControlAudioState = components["schemas"]["AudioState"];
export type ControlAudioMeasurement = components["schemas"]["AudioMeasurement"];
export type ControlAudioPatch = components["schemas"]["AudioPatch"];
export type ControlAudioChannel = components["schemas"]["AudioChannel"];
export type ControlPairingState = components["schemas"]["PairingState"];
export type ControlPairResponse = components["schemas"]["PairResponse"];
export type ControlPatch = components["schemas"]["Patch"];
export type ControlPatchUpdate = components["schemas"]["PatchUpdate"];
export type ControlPatchEditor = components["schemas"]["PatchEditor"];
export type ControlPatchSourceRequest = components["schemas"]["PatchSourceRequest"];
export type ControlPatchInstrument = components["schemas"]["PatchInstrument"];
export type ControlFxChain = components["schemas"]["FxChain"];
export type ControlFxInsert = components["schemas"]["FxInsert"];
export type ControlFxInsertRequest = components["schemas"]["FxInsertRequest"];
export type ControlFxInsertUpdate = components["schemas"]["FxInsertUpdate"];
export type ControlPlugin = components["schemas"]["Plugin"];
export type ControlPluginList = components["schemas"]["PluginList"];
export type ControlMidiState = components["schemas"]["MidiState"];
export type ControlMidiInput = components["schemas"]["MidiInput"];
export type ControlMidiPort = components["schemas"]["MidiPort"];
export type ControlMidiPadTest = components["schemas"]["MidiPadTest"];
export type ControlMidiInputUpdate = components["schemas"]["MidiInputUpdate"];

/** Raised for any non-2xx response or an unusable payload. */
export class ApiError extends Error
{
    readonly status: number;

    constructor (message: string, status = 0)
    {
        super (message);
        this.name = "ApiError";
        this.status = status;
    }
}

export interface ControlApiOptions
{
    /** Origin of the control server, e.g. `http://127.0.0.1:8080`. */
    baseUrl: string;
    /** Pairing token; sent as `X-Instrument-Hub-Token` when set. */
    token?: string | null;
    /** Injectable fetch, used by tests. Defaults to the global fetch. */
    fetch?: typeof globalThis.fetch;
}

/**
    Typed façade over the control API.

    The request and response types come straight from the generated
    `schema.d.ts` (produced from `shared/api/openapi.yaml`), so a change to the
    OpenAPI document breaks the build instead of surfacing at runtime.
*/
export interface ControlApi
{
    readonly baseUrl: string;

    /** Pairing token used by every request, or null when unpaired. */
    readonly token: string | null;

    /** Replace the pairing token; subsequent requests use the new value. */
    setToken (token: string | null): void;

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

    //==========================================================================
    // Per-patch insert chain (T121)

    /** Read one patch's ordered inserts and latency budget. */
    getFxChain (slot: number, options?: { signal?: AbortSignal }): Promise<ControlFxChain>;

    /** Append an effect (or insert it at `position`). */
    addFxInsert (slot: number,
                 insert: ControlFxInsertRequest,
                 options?: { signal?: AbortSignal }): Promise<ControlFxChain>;

    /** Replace the whole chain atomically. */
    replaceFxChain (slot: number,
                    chain: readonly ControlFxInsertRequest[],
                    options?: { signal?: AbortSignal }): Promise<ControlFxChain>;

    /** Bypass, re-balance, re-parameterise or move one insert. */
    updateFxInsert (slot: number,
                    index: number,
                    update: ControlFxInsertUpdate,
                    options?: { signal?: AbortSignal }): Promise<ControlFxChain>;

    /** Remove one insert. */
    removeFxInsert (slot: number,
                    index: number,
                    options?: { signal?: AbortSignal }): Promise<ControlFxChain>;

    /** Reorder the chain; `order[0]` becomes the first insert. */
    reorderFxChain (slot: number,
                    order: readonly number[],
                    options?: { signal?: AbortSignal }): Promise<ControlFxChain>;

    //==========================================================================
    // Plugin inventory (T122)

    /** Read the installed VST3 inventory, optionally filtered by `search`. */
    listPlugins (options?: { search?: string; signal?: AbortSignal }): Promise<ControlPluginList>;

    /** Start a background rescan and return the inventory as it was then. */
    rescanPlugins (options?: { signal?: AbortSignal }): Promise<ControlPluginList>;

    //==========================================================================
    // MIDI inputs and the pad test (T126)

    /** Read the MIDI inputs, Launchpad/Keystation status and pad-test state. */
    getMidi (options?: { signal?: AbortSignal }): Promise<ControlMidiState>;

    /** Enable or disable one MIDI input, returning the new state. */
    setMidiInput (id: string,
                  enabled: boolean,
                  options?: { signal?: AbortSignal }): Promise<ControlMidiState>;

    /** Light the Launchpad grid for ~900 ms. */
    runMidiPadTest (options?: { signal?: AbortSignal }): Promise<ControlMidiState>;

    /** Read the audio device settings and live latency readout. */
    getAudio (options?: { signal?: AbortSignal }): Promise<ControlAudioState>;

    /**
        Change one or more audio settings.

        Resolves with the state after the change, including the new buffer /
        device latency readout.
    */
    updateAudio (patch: ControlAudioPatch,
                 options?: { signal?: AbortSignal }): Promise<ControlAudioState>;

    /** Run the loopback latency probe (blocks for a few hundred ms). */
    measureLatency (options?: { signal?: AbortSignal }): Promise<ControlAudioMeasurement>;

    //==========================================================================
    // LAN pairing (T101)

    /** Read LAN mode, the live 6-digit code and the QR payload. */
    getPairing (options?: { signal?: AbortSignal }): Promise<ControlPairingState>;

    /** Turn LAN mode on/off (loopback-only management endpoint). */
    setLanMode (enabled: boolean,
                options?: { signal?: AbortSignal }): Promise<ControlPairingState>;

    /** Exchange a 6-digit code for a token. */
    pair (code: string,
          options?: { signal?: AbortSignal; clientName?: string }): Promise<ControlPairResponse>;

    /** Revoke every issued pairing token. */
    revokePairing (options?: { signal?: AbortSignal }): Promise<ControlPairingState>;
}

function describeError (value: unknown): string
{
    if (value !== null && typeof value === "object" && "error" in value)
    {
        const message = (value as { error?: unknown }).error;

        if (typeof message === "string" && message.length > 0)
            return message;
    }

    return "";
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

/** Rejects chain payloads the editor cannot render. */
function isFxChain (value: unknown): value is ControlFxChain
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlFxChain>;

    return typeof candidate.slot === "number"
        && typeof candidate.instrumentLatencySamples === "number"
        && typeof candidate.fxLatencySamples === "number"
        && typeof candidate.latencySamples === "number"
        && typeof candidate.latencyMs === "number"
        && Array.isArray (candidate.inserts)
        && candidate.inserts.every (insert =>
               insert !== null
               && typeof insert === "object"
               && typeof insert.index === "number"
               && typeof insert.type === "string"
               && typeof insert.kind === "string"
               && typeof insert.id === "string"
               && typeof insert.name === "string"
               && typeof insert.bypassed === "boolean"
               && typeof insert.wet === "number"
               && typeof insert.latencySamples === "number"
               && insert.parameters !== null
               && typeof insert.parameters === "object");
}

/** Rejects inventory payloads the plugin browser cannot render. */
function isPluginList (value: unknown): value is ControlPluginList
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlPluginList>;

    return Array.isArray (candidate.plugins)
        && typeof candidate.count === "number"
        && typeof candidate.total === "number"
        && typeof candidate.search === "string"
        && typeof candidate.scanning === "boolean"
        && typeof candidate.scanned === "boolean"
        && typeof candidate.lastScanMs === "number"
        && typeof candidate.error === "string"
        && candidate.plugins.every (plugin =>
               plugin !== null
               && typeof plugin === "object"
               && typeof plugin.id === "string"
               && typeof plugin.name === "string"
               && typeof plugin.vendor === "string"
               && typeof plugin.version === "string"
               && typeof plugin.category === "string"
               && typeof plugin.isInstrument === "boolean"
               && typeof plugin.isEffect === "boolean"
               && typeof plugin.format === "string"
               && typeof plugin.path === "string");
}

/** Rejects MIDI payloads the page cannot render. */
function isMidiState (value: unknown): value is ControlMidiState
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlMidiState>;

    const isPort = (port: ControlMidiPort | undefined): boolean =>
        port !== null
        && port !== undefined
        && typeof port.name === "string"
        && typeof port.port === "string"
        && typeof port.connected === "boolean"
        && typeof port.handshake === "boolean"
        && typeof port.inputName === "string"
        && typeof port.outputName === "string"
        && typeof port.error === "string";

    return Array.isArray (candidate.inputs)
        && typeof candidate.inputCount === "number"
        && candidate.inputs.every (input =>
               input !== null
               && typeof input === "object"
               && typeof input.id === "string"
               && typeof input.name === "string"
               && typeof input.role === "string"
               && typeof input.port === "string"
               && typeof input.enabled === "boolean"
               && typeof input.open === "boolean"
               && typeof input.events === "number"
               && typeof input.lastEvent === "string")
        && isPort (candidate.launchpad)
        && isPort (candidate.keystation)
        && candidate.padTest !== null
        && candidate.padTest !== undefined
        && typeof candidate.padTest.active === "boolean"
        && typeof candidate.padTest.lit === "number"
        && typeof candidate.padTest.durationMs === "number"
        && typeof candidate.error === "string";
}

/** Rejects audio payloads the panel cannot render. */
function isAudioState (value: unknown): value is ControlAudioState
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlAudioState>;

    return typeof candidate.open === "boolean"
        && typeof candidate.deviceType === "string"
        && typeof candidate.deviceName === "string"
        && Array.isArray (candidate.deviceTypes)
        && Array.isArray (candidate.devices)
        && typeof candidate.sampleRate === "number"
        && typeof candidate.bufferSize === "number"
        && Array.isArray (candidate.sampleRates)
        && Array.isArray (candidate.bufferSizes)
        && Array.isArray (candidate.inputChannels)
        && Array.isArray (candidate.outputChannels)
        && typeof candidate.latency?.bufferMs === "number"
        && typeof candidate.latency?.deviceMs === "number"
        && typeof candidate.latency?.chainMs === "number"
        && typeof candidate.latency?.totalMs === "number"
        && typeof candidate.xruns === "number";
}

function isAudioMeasurement (value: unknown): value is ControlAudioMeasurement
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlAudioMeasurement>;

    return typeof candidate.valid === "boolean"
        && typeof candidate.roundTripSamples === "number"
        && typeof candidate.roundTripMs === "number"
        && typeof candidate.expectedMs === "number"
        && typeof candidate.confidence === "number"
        && typeof candidate.sampleRate === "number"
        && typeof candidate.bufferSize === "number"
        && typeof candidate.error === "string";
}

/** Rejects pairing payloads the panel cannot render. */
function isPairingState (value: unknown): value is ControlPairingState
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlPairingState>;

    return typeof candidate.lanEnabled === "boolean"
        && typeof candidate.address === "string"
        && typeof candidate.port === "number"
        && typeof candidate.code === "string"
        && typeof candidate.codeExpiresInMs === "number"
        && typeof candidate.payload === "string"
        && typeof candidate.pairedClients === "number";
}

function isPairResponse (value: unknown): value is ControlPairResponse
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlPairResponse>;

    return typeof candidate.token === "string"
        && candidate.token.length > 0
        && typeof candidate.pairedClients === "number";
}

export function createControlApi (options: ControlApiOptions): ControlApi
{
    const baseUrl = options.baseUrl.replace (/\/+$/, "");
    let tokenValue = options.token ?? null;

    const client = createClient<paths> ({
        baseUrl,
        ...(options.fetch !== undefined ? { fetch: options.fetch } : {}),
    });

    // The token changes when the user pairs or forgets a tablet, so it is
    // injected per request instead of being baked into the client headers.
    client.use ({
        onRequest ({ request })
        {
            if (tokenValue !== null && tokenValue.length > 0)
                request.headers.set ("X-Instrument-Hub-Token", tokenValue);

            return request;
        },
    });

    function requireState (data: ControlState | undefined,
                           error: unknown,
                           status: number,
                           what: string): ControlState
    {
        if (data !== undefined)
        {
            if (! isControlState (data))
                throw new ApiError (`${what} returned an unsupported payload`, status);

            return data;
        }

        const message = describeError (error);

        throw new ApiError (
            message.length > 0 ? message : `${what} failed with HTTP ${status}`,
            status);
    }

    function requireChain (data: ControlFxChain | undefined,
                           error: unknown,
                           status: number,
                           what: string): ControlFxChain
    {
        if (data !== undefined)
        {
            if (! isFxChain (data))
                throw new ApiError (`${what} returned an unsupported payload`, status);

            return data;
        }

        const message = describeError (error);

        throw new ApiError (
            message.length > 0 ? message : `${what} failed with HTTP ${status}`,
            status);
    }

    function requireEditor (data: ControlPatchEditor | undefined,
                            error: unknown,
                            status: number,
                            what: string): ControlPatchEditor
    {
        if (data !== undefined
            && typeof data.slot === "number"
            && typeof data.open === "boolean")
            return data;

        const message = describeError (error);

        throw new ApiError (
            message.length > 0 ? message : `${what} failed with HTTP ${status}`,
            status);
    }

    function requirePluginList (data: ControlPluginList | undefined,
                                error: unknown,
                                status: number,
                                what: string): ControlPluginList
    {
        if (data !== undefined)
        {
            if (! isPluginList (data))
                throw new ApiError (`${what} returned an unsupported payload`, status);

            return data;
        }

        const message = describeError (error);

        throw new ApiError (
            message.length > 0 ? message : `${what} failed with HTTP ${status}`,
            status);
    }

    return {
        baseUrl,

        get token ()
        {
            return tokenValue;
        },

        setToken (token)
        {
            tokenValue = token !== null && token.length > 0 ? token : null;
        },

        async getState (requestOptions)
        {
            const { data, error, response } = await client.GET ("/state", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (data, error, response.status, "GET /state");
        },

        async armSlot (slot, requestOptions)
        {
            const { data, error, response } = await client.POST ("/arm", {
                body: { slot },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (data, error, response.status, "POST /arm");
        },

        async controlTransport (patch, requestOptions)
        {
            const { data, error, response } = await client.POST ("/transport", {
                body: patch,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isTransport (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `POST /transport failed with HTTP ${response.status}`,
                response.status);
        },

        async updatePatch (slot, patch, requestOptions)
        {
            const { data, error, response } = await client.PATCH ("/patches/{slot}", {
                params: { path: { slot } },
                body: patch,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (data, error, response.status, `PATCH /patches/${String (slot)}`);
        },

        async changePatchSource (slot, source, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/source", {
                params: { path: { slot } },
                body: source,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireState (data, error, response.status,
                                 `POST /patches/${String (slot)}/source`);
        },

        async openPatchEditor (slot, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/editor", {
                params: { path: { slot } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireEditor (data, error, response.status,
                                  `POST /patches/${String (slot)}/editor`);
        },

        async closePatchEditor (slot, requestOptions)
        {
            const { data, error, response } = await client.DELETE ("/patches/{slot}/editor", {
                params: { path: { slot } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireEditor (data, error, response.status,
                                  `DELETE /patches/${String (slot)}/editor`);
        },

        async getFxChain (slot, requestOptions)
        {
            const { data, error, response } = await client.GET ("/patches/{slot}/fx", {
                params: { path: { slot } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireChain (data, error, response.status,
                                 `GET /patches/${String (slot)}/fx`);
        },

        async addFxInsert (slot, insert, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/fx", {
                params: { path: { slot } },
                body: insert,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireChain (data, error, response.status,
                                 `POST /patches/${String (slot)}/fx`);
        },

        async replaceFxChain (slot, chain, requestOptions)
        {
            const { data, error, response } = await client.PUT ("/patches/{slot}/fx", {
                params: { path: { slot } },
                body: { chain: [...chain] },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireChain (data, error, response.status,
                                 `PUT /patches/${String (slot)}/fx`);
        },

        async updateFxInsert (slot, index, update, requestOptions)
        {
            const { data, error, response } = await client.PATCH ("/patches/{slot}/fx/{index}", {
                params: { path: { slot, index } },
                body: update,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireChain (data, error, response.status,
                                 `PATCH /patches/${String (slot)}/fx/${String (index)}`);
        },

        async removeFxInsert (slot, index, requestOptions)
        {
            const { data, error, response } = await client.DELETE ("/patches/{slot}/fx/{index}", {
                params: { path: { slot, index } },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireChain (data, error, response.status,
                                 `DELETE /patches/${String (slot)}/fx/${String (index)}`);
        },

        async reorderFxChain (slot, order, requestOptions)
        {
            const { data, error, response } = await client.POST ("/patches/{slot}/fx/reorder", {
                params: { path: { slot } },
                body: { order: [...order] },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireChain (data, error, response.status,
                                 `POST /patches/${String (slot)}/fx/reorder`);
        },

        async listPlugins (requestOptions)
        {
            const search = requestOptions?.search;

            const { data, error, response } = await client.GET ("/plugins", {
                ...(search !== undefined && search.length > 0 ? { params: { query: { search } } } : {}),
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requirePluginList (data, error, response.status, "GET /plugins");
        },

        async rescanPlugins (requestOptions)
        {
            const { data, error, response } = await client.POST ("/plugins/rescan", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requirePluginList (data, error, response.status, "POST /plugins/rescan");
        },

        async getMidi (requestOptions)
        {
            const { data, error, response } = await client.GET ("/midi", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isMidiState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `GET /midi failed with HTTP ${response.status}`,
                response.status);
        },

        async setMidiInput (id, enabled, requestOptions)
        {
            const { data, error, response } = await client.POST ("/midi/inputs", {
                body: { id, enabled },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isMidiState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0
                    ? message
                    : `POST /midi/inputs failed with HTTP ${response.status}`,
                response.status);
        },

        async runMidiPadTest (requestOptions)
        {
            const { data, error, response } = await client.POST ("/midi/pad-test", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isMidiState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0
                    ? message
                    : `POST /midi/pad-test failed with HTTP ${response.status}`,
                response.status);
        },

        async getAudio (requestOptions)
        {
            const { data, error, response } = await client.GET ("/audio", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isAudioState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `GET /audio failed with HTTP ${response.status}`,
                response.status);
        },

        async updateAudio (patch, requestOptions)
        {
            const { data, error, response } = await client.POST ("/audio", {
                body: patch,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isAudioState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `POST /audio failed with HTTP ${response.status}`,
                response.status);
        },

        async measureLatency (requestOptions)
        {
            const { data, error, response } = await client.POST ("/audio/measure", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isAudioMeasurement (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `POST /audio/measure failed with HTTP ${response.status}`,
                response.status);
        },

        async getPairing (requestOptions)
        {
            const { data, error, response } = await client.GET ("/pairing", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isPairingState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `GET /pairing failed with HTTP ${response.status}`,
                response.status);
        },

        async setLanMode (enabled, requestOptions)
        {
            const { data, error, response } = await client.POST ("/pairing/lan", {
                body: { enabled },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isPairingState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `POST /pairing/lan failed with HTTP ${response.status}`,
                response.status);
        },

        async pair (code, requestOptions)
        {
            const { data, error, response } = await client.POST ("/pair", {
                body: {
                    code,
                    ...(requestOptions?.clientName !== undefined
                        ? { clientName: requestOptions.clientName }
                        : {}),
                },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isPairResponse (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `POST /pair failed with HTTP ${response.status}`,
                response.status);
        },

        async revokePairing (requestOptions)
        {
            const { data, error, response } = await client.POST ("/pairing/revoke", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined && isPairingState (data))
                return data;

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `POST /pairing/revoke failed with HTTP ${response.status}`,
                response.status);
        },
    };
}
