import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type {
    ControlAudioMeasurement,
    ControlAudioPatch,
    ControlAudioState,
} from "./types";

/** Audio device settings and the loopback latency probe. */
export interface AudioEndpoints
{
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

function requireAudioState (guard: RequestGuard,
                            data: ControlAudioState | undefined,
                            error: unknown,
                            status: number,
                            what: string): ControlAudioState
{
    if (data !== undefined && isAudioState (data))
        return data;

    return guard.fail (error, status, what);
}

function requireAudioMeasurement (guard: RequestGuard,
                                  data: ControlAudioMeasurement | undefined,
                                  error: unknown,
                                  status: number,
                                  what: string): ControlAudioMeasurement
{
    if (data !== undefined && isAudioMeasurement (data))
        return data;

    return guard.fail (error, status, what);
}

export function createAudioEndpoints (client: Client<paths>,
                                      guard: RequestGuard): AudioEndpoints
{
    return {
        async getAudio (requestOptions)
        {
            const { data, error, response } = await client.GET ("/audio", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireAudioState (guard, data, error, response.status, "GET /audio");
        },

        async updateAudio (patch, requestOptions)
        {
            const { data, error, response } = await client.POST ("/audio", {
                body: patch,
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireAudioState (guard, data, error, response.status, "POST /audio");
        },

        async measureLatency (requestOptions)
        {
            const { data, error, response } = await client.POST ("/audio/measure", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireAudioMeasurement (guard, data, error, response.status,
                                            "POST /audio/measure");
        },
    };
}
