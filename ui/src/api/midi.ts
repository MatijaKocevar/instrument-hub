import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type { ControlMidiPort, ControlMidiState } from "./types";

/** MIDI inputs and the Launchpad pad test. */
export interface MidiEndpoints
{
    /** Read the MIDI inputs, Launchpad/Keystation status and pad-test state. */
    getMidi (options?: { signal?: AbortSignal }): Promise<ControlMidiState>;

    /** Enable or disable one MIDI input, returning the new state. */
    setMidiInput (id: string,
                  enabled: boolean,
                  options?: { signal?: AbortSignal }): Promise<ControlMidiState>;

    /** Light the Launchpad grid for ~900 ms. */
    runMidiPadTest (options?: { signal?: AbortSignal }): Promise<ControlMidiState>;
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

function requireMidiState (guard: RequestGuard,
                           data: ControlMidiState | undefined,
                           error: unknown,
                           status: number,
                           what: string): ControlMidiState
{
    if (data !== undefined && isMidiState (data))
        return data;

    return guard.fail (error, status, what);
}

export function createMidiEndpoints (client: Client<paths>,
                                     guard: RequestGuard): MidiEndpoints
{
    return {
        async getMidi (requestOptions)
        {
            const { data, error, response } = await client.GET ("/midi", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireMidiState (guard, data, error, response.status, "GET /midi");
        },

        async setMidiInput (id, enabled, requestOptions)
        {
            const { data, error, response } = await client.POST ("/midi/inputs", {
                body: { id, enabled },
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireMidiState (guard, data, error, response.status, "POST /midi/inputs");
        },

        async runMidiPadTest (requestOptions)
        {
            const { data, error, response } = await client.POST ("/midi/pad-test", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requireMidiState (guard, data, error, response.status, "POST /midi/pad-test");
        },
    };
}
