import createClient from "openapi-fetch";

import type { components, paths } from "./schema";

/** The complete control state returned by `GET /state`. */
export type ControlState = components["schemas"]["State"];
export type ControlSlot = components["schemas"]["Slot"];
export type ControlSettings = components["schemas"]["Settings"];
export type ControlTransport = components["schemas"]["Transport"];
export type ControlAudioState = components["schemas"]["AudioState"];
export type ControlAudioMeasurement = components["schemas"]["AudioMeasurement"];
export type ControlAudioPatch = components["schemas"]["AudioPatch"];
export type ControlAudioChannel = components["schemas"]["AudioChannel"];

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

    /** Read one consistent state snapshot. */
    getState (options?: { signal?: AbortSignal }): Promise<ControlState>;

    /**
        Arm `slot`, or disarm with `null` / a negative value.

        Resolves with the state after the request, so the caller can reconcile
        its optimistic highlight with the engine.
    */
    armSlot (slot: number | null, options?: { signal?: AbortSignal }): Promise<ControlState>;

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
               && typeof slot.armed === "boolean")
        && typeof candidate.settings?.masterGain === "number"
        && typeof candidate.transport?.bpm === "number";
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

export function createControlApi (options: ControlApiOptions): ControlApi
{
    const baseUrl = options.baseUrl.replace (/\/+$/, "");

    const headers: Record<string, string> = {};

    if (options.token !== undefined && options.token !== null && options.token.length > 0)
        headers["X-Instrument-Hub-Token"] = options.token;

    const client = createClient<paths> ({
        baseUrl,
        headers,
        ...(options.fetch !== undefined ? { fetch: options.fetch } : {}),
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

    return {
        baseUrl,

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
    };
}
