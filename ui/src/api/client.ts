import createClient from "openapi-fetch";

import { createAudioEndpoints, type AudioEndpoints } from "./audio";
import { createFxEndpoints, type FxEndpoints } from "./fx";
import { createMidiEndpoints, type MidiEndpoints } from "./midi";
import { createPairingEndpoints, type PairingEndpoints } from "./pairing";
import { createPluginEndpoints, type PluginEndpoints } from "./plugins";
import type { paths } from "./schema";
import { createStateEndpoints, type StateEndpoints } from "./state";

export type {
    ControlAudioChannel,
    ControlAudioMeasurement,
    ControlAudioPatch,
    ControlAudioState,
    ControlFxChain,
    ControlFxInsert,
    ControlFxInsertRequest,
    ControlFxInsertUpdate,
    ControlMidiInput,
    ControlMidiInputUpdate,
    ControlMidiPadTest,
    ControlMidiPort,
    ControlMidiState,
    ControlPairResponse,
    ControlPairingState,
    ControlPatch,
    ControlPatchEditor,
    ControlPatchInstrument,
    ControlPatchSourceRequest,
    ControlPatchStateCapture,
    ControlPatchUpdate,
    ControlPlugin,
    ControlPluginList,
    ControlSettings,
    ControlSlot,
    ControlState,
    ControlTransport,
    ControlTransportPatch,
} from "./types";

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
    The error core a resource module needs.

    The client owns the transport and `ApiError`; the resource owns the payload
    checks and hands failures back through this guard. Keeping the dependency
    this way means the transport core can compose the resource without the two
    modules importing each other at runtime.
*/
export interface RequestGuard
{
    /** Throw the core's error for a failed request. */
    fail (error: unknown, status: number, what: string): never;
    /** Throw the core's error for a payload the UI cannot render. */
    reject (message: string, status: number): never;
}

/**
    Typed façade over the control API.

    The request and response types come straight from the generated
    `schema.d.ts` (produced from `shared/api/openapi.yaml`), so a change to the
    OpenAPI document breaks the build instead of surfacing at runtime.

    Each resource group lives in its own module; the façade only composes them
    behind the shared transport and token.
*/
export interface ControlApi
    extends StateEndpoints,
            FxEndpoints,
            PluginEndpoints,
            MidiEndpoints,
            AudioEndpoints,
            PairingEndpoints
{
    readonly baseUrl: string;

    /** Pairing token used by every request, or null when unpaired. */
    readonly token: string | null;

    /** Replace the pairing token; subsequent requests use the new value. */
    setToken (token: string | null): void;
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

/** Throw the core error for a failed request. */
function fail (error: unknown, status: number, what: string): never
{
    const message = describeError (error);

    throw new ApiError (
        message.length > 0 ? message : `${what} failed with HTTP ${status}`,
        status);
}

/** Throw the core error for a payload the UI cannot render. */
function reject (message: string, status: number): never
{
    throw new ApiError (message, status);
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

    const guard: RequestGuard = { fail, reject };

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

        ...createStateEndpoints (client, guard),
        ...createFxEndpoints (client, guard),
        ...createPluginEndpoints (client, guard),
        ...createMidiEndpoints (client, guard),
        ...createAudioEndpoints (client, guard),
        ...createPairingEndpoints (client, guard),
    };
}
