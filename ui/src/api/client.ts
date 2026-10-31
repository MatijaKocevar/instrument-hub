import createClient from "openapi-fetch";

import type { components, paths } from "./schema";

/** The complete control state returned by `GET /state`. */
export type ControlState = components["schemas"]["State"];
export type ControlSlot = components["schemas"]["Slot"];
export type ControlSettings = components["schemas"]["Settings"];
export type ControlTransport = components["schemas"]["Transport"];

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

    return {
        baseUrl,

        async getState (requestOptions)
        {
            const { data, error, response } = await client.GET ("/state", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            if (data !== undefined)
            {
                if (! isControlState (data))
                    throw new ApiError ("GET /state returned an unsupported payload", response.status);

                return data;
            }

            const message = describeError (error);

            throw new ApiError (
                message.length > 0 ? message : `GET /state failed with HTTP ${response.status}`,
                response.status);
        },
    };
}
