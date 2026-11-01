// Fixture HTTP server for the headless snapshot harness.
//
// Speaks the control API from a mutable fixture, so the harness exercises the
// same HTTP path the browser uses. Supports state, arming, audio, transport,
// pairing, MIDI, FX chains, patch edits and the plug-in inventory.

import {
    createServer,
    type IncomingMessage,
    type Server,
    type ServerResponse,
} from "node:http";
import type { AddressInfo } from "node:net";

import type {
    ControlAudioPatch,
    ControlAudioState,
    ControlFxChain,
    ControlFxInsert,
    ControlMidiState,
    ControlPairingState,
    ControlPlugin,
    ControlState,
    ControlTransportPatch,
} from "../api/client";
import {
    AUDIO,
    CHAIN_INSTRUMENT_LATENCY,
    COMPRESSOR_LATENCY,
    FIXTURE,
    MIDI,
    PAIRING_OFF,
    PAIRING_ON,
    PLUGIN_FIXTURE,
    fixtureInsert,
    recalculateChain,
} from "./fixtures";

export interface FixtureServer
{
    baseUrl: string;
    readonly stateRequests: number;
    readonly armRequests: readonly { slot: number | null }[];
    readonly audioRequests: readonly ControlAudioPatch[];
    readonly transportRequests: readonly ControlTransportPatch[];
    readonly measureRequests: number;
    readonly audio: ControlAudioState;
    readonly pairing: ControlPairingState;
    readonly pairRequests: readonly string[];
    readonly lanRequests: readonly boolean[];
    readonly patchRequests: readonly { slot: number; patch: Record<string, unknown> }[];
    readonly fxRequests: readonly { method: string; slot: number; index: number; body: Record<string, unknown> }[];
    readonly sourceRequests: readonly { slot: number; body: Record<string, unknown> }[];
    readonly editorRequests: readonly { slot: number; open: boolean }[];
    readonly stateCaptureRequests: readonly { slot: number }[];
    readonly pluginRequests: readonly string[];
    readonly midiRequests: readonly { id: string; enabled: boolean }[];
    readonly padTestRequests: number;
    readonly midi: ControlMidiState;
    readonly chain: ControlFxChain;
    close: () => Promise<void>;
}

export interface FixtureOptions
{
    /** Make the first N `GET /state` requests fail with HTTP 503. */
    failFirst?: number;
    /** Delay the `POST /arm` response so the optimistic highlight is observable. */
    armDelayMs?: number;
}

function listen (server: Server): Promise<number>
{
    return new Promise ((resolve, reject) =>
    {
        server.once ("error", reject);
        server.listen (0, "127.0.0.1", () =>
        {
            const address = server.address ();

            if (address === null || typeof address === "string")
            {
                reject (new Error ("fixture server did not report a TCP address"));
                return;
            }

            resolve ((address as AddressInfo).port);
        });
    });
}

function cloneState (state: ControlState): ControlState
{
    return JSON.parse (JSON.stringify (state)) as ControlState;
}

function cloneAudio (audio: ControlAudioState): ControlAudioState
{
    return JSON.parse (JSON.stringify (audio)) as ControlAudioState;
}

function cloneMidi (midi: ControlMidiState): ControlMidiState
{
    return JSON.parse (JSON.stringify (midi)) as ControlMidiState;
}

function recalculateLatency (audio: ControlAudioState): void
{
    const bufferMs = (audio.bufferSize * 1000) / Math.max (1, audio.sampleRate);

    audio.latency = {
        ...audio.latency,
        bufferMs,
        totalMs: bufferMs + audio.latency.deviceMs + audio.latency.chainMs,
    };
}

/**
    Serve the control API from a mutable fixture.

    Supports `GET /state` (with optional early failures) and `POST /arm`, so
    the test exercises the same HTTP path the browser uses.
*/
export async function startFixtureServer (options: FixtureOptions = {}): Promise<FixtureServer>
{
    const failFirst = options.failFirst ?? 0;
    const armDelayMs = options.armDelayMs ?? 0;
    const state = cloneState (FIXTURE);
    const audio = cloneAudio (AUDIO);
    const armRequests: { slot: number | null }[] = [];
    const audioRequests: ControlAudioPatch[] = [];
    const transportRequests: ControlTransportPatch[] = [];
    const pairRequests: string[] = [];
    const lanRequests: boolean[] = [];
    const patchRequests: { slot: number; patch: Record<string, unknown> }[] = [];
    const fxRequests: { method: string; slot: number; index: number; body: Record<string, unknown> }[] = [];
    const sourceRequests: { slot: number; body: Record<string, unknown> }[] = [];
    const editorRequests: { slot: number; open: boolean }[] = [];
    const stateCaptureRequests: { slot: number }[] = [];
    const pluginRequests: string[] = [];
    const midi = cloneMidi (MIDI);
    const midiRequests: { id: string; enabled: boolean }[] = [];
    let padTestRequests = 0;
    let pluginPlugins: ControlPlugin[] = PLUGIN_FIXTURE.plugins.map (plugin => ({ ...plugin }));
    let pluginScanning = false;
    let pluginPendingScan = false;
    const chains = new Map<number, ControlFxChain> ();

    const chainFor = (slot: number): ControlFxChain =>
    {
        const existing = chains.get (slot);

        if (existing !== undefined)
            return existing;

        const chain: ControlFxChain = {
            slot,
            instrumentLatencySamples: slot === 3 ? CHAIN_INSTRUMENT_LATENCY : 0,
            fxLatencySamples: 0,
            latencySamples: 0,
            latencyMs: 0,
            inserts: slot === 3
                ? [
                      fixtureInsert (0, "eq", "eq", "EQ",
                                    0, { lowGainDb: 2, midGainDb: 0, highGainDb: -1 }),
                      fixtureInsert (1, "compressor", "compressor", "Compressor",
                                     COMPRESSOR_LATENCY, { thresholdDb: -24, ratio: 3 }),
                  ]
                : [],
        };

        recalculateChain (chain);
        chains.set (slot, chain);
        return chain;
    };
    let stateRequests = 0;
    let measureRequests = 0;
    let pairing = { ...PAIRING_OFF };
    let pairingToken: string | null = null;

    const suppliedToken = (request: IncomingMessage): string | null =>
    {
        const value = request.headers["x-instrument-hub-token"];

        return typeof value === "string" ? value : null;
    };

    const authorized = (request: IncomingMessage): boolean =>
        ! pairing.lanEnabled || suppliedToken (request) === pairingToken;

    const sendPairing = (response: ServerResponse): void =>
    {
        response.statusCode = 200;
        response.end (JSON.stringify (pairing));
    };

    const server = createServer ((request, response) =>
    {
        response.setHeader ("content-type", "application/json");

        const isPairingManagement = request.url === "/pairing"
            || request.url === "/pairing/lan"
            || request.url === "/pairing/revoke";

        if (pairing.lanEnabled && ! isPairingManagement && request.url !== "/pair"
            && ! authorized (request))
        {
            response.statusCode = 401;
            response.end (JSON.stringify ({ error: "unauthorized" }));
            return;
        }

        if (isPairingManagement && pairing.lanEnabled && ! authorized (request))
        {
            response.statusCode = 401;
            response.end (JSON.stringify ({ error: "unauthorized" }));
            return;
        }

        if (request.method === "GET" && request.url === "/pairing")
        {
            sendPairing (response);
            return;
        }

        if (request.method === "POST" && request.url === "/pairing/lan")
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                const enabled = (JSON.parse (body) as { enabled?: unknown }).enabled === true;

                lanRequests.push (enabled);

                if (enabled)
                {
                    pairing = { ...PAIRING_ON };
                    pairingToken = null;
                }
                else
                {
                    pairing = { ...PAIRING_OFF };
                    pairingToken = null;
                }

                sendPairing (response);
            });

            return;
        }

        if (request.method === "POST" && request.url === "/pairing/revoke")
        {
            pairingToken = null;
            pairing = { ...pairing, pairedClients: 0 };
            sendPairing (response);
            return;
        }

        if (request.method === "POST" && request.url === "/pair")
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                const parsed = JSON.parse (body) as { code?: unknown; clientName?: unknown };
                const code = typeof parsed.code === "string" ? parsed.code : "";
                pairRequests.push (code);

                if (! pairing.lanEnabled)
                {
                    response.statusCode = 403;
                    response.end (JSON.stringify ({ error: "LAN mode is off" }));
                    return;
                }

                if (code !== pairing.code)
                {
                    response.statusCode = 401;
                    response.end (JSON.stringify ({ error: "invalid pairing code" }));
                    return;
                }

                pairingToken = "fixture-pairing-token";

                const rotatedCode = pairing.code === "246810" ? "135790" : "246810";
                pairing = {
                    ...pairing,
                    code: rotatedCode,
                    codeExpiresInMs: 300_000,
                    payload: `http://192.168.1.20:8080/remote?code=${rotatedCode}`,
                    pairedClients: pairing.pairedClients + 1,
                };

                response.statusCode = 200;
                response.end (JSON.stringify ({
                    token: pairingToken,
                    pairedClients: pairing.pairedClients,
                }));
            });

            return;
        }

        const url = request.url ?? "";
        const fxMatch = /^\/patches\/(\d+)\/fx$/.exec (url);
        const fxReorderMatch = /^\/patches\/(\d+)\/fx\/reorder$/.exec (url);
        const fxIndexMatch = /^\/patches\/(\d+)\/fx\/(\d+)$/.exec (url);
        const sourceMatch = /^\/patches\/(\d+)\/source$/.exec (url);
        const editorMatch = /^\/patches\/(\d+)\/editor$/.exec (url);
        const stateCaptureMatch = /^\/patches\/(\d+)\/state\/capture$/.exec (url);

        const readBody = (done: (parsed: Record<string, unknown>) => void): void =>
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                try
                {
                    const parsed = JSON.parse (body) as unknown;

                    done (parsed !== null && typeof parsed === "object" && ! Array.isArray (parsed)
                        ? parsed as Record<string, unknown>
                        : {});
                }
                catch
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "body must be a JSON object" }));
                }
            });
        };

        const respondChain = (chain: ControlFxChain): void =>
        {
            response.statusCode = 200;
            response.end (JSON.stringify (chain));
        };

        const insertFromRequest = (body: Record<string, unknown>,
                                   chainLength: number): ControlFxInsert =>
        {
            const type = (typeof body.type === "string"
                ? body.type
                : "eq") as ControlFxInsert["type"];
            const wet = typeof body.wet === "number" ? body.wet : 1;
            const latency = type === "compressor" ? COMPRESSOR_LATENCY
                : type === "delay" ? 96
                    : 0;

            return {
                index: chainLength,
                type,
                kind: type === "vst3" ? "vst3" : "builtin",
                id: typeof body.id === "string" && body.id.length > 0
                    ? body.id
                    : (typeof body.path === "string" ? body.path : type),
                name: typeof body.name === "string" && body.name.length > 0
                    ? body.name
                    : type,
                bypassed: body.bypassed === true,
                wet,
                dry: 1 - wet,
                latencySamples: latency,
                parameters: (body.parameters ?? {}) as Record<string, number>,
            };
        };

        const findPatch = (slot: number): ControlState["families"][number]["patches"][number] | null =>
        {
            for (const family of state.families)
                for (const patch of family.patches)
                    if (patch.slot === slot)
                        return patch;

            return null;
        };

        if (fxMatch !== null)
        {
            const slot = Number.parseInt (fxMatch[1] ?? "", 10);
            const chain = chainFor (slot);

            if (request.method === "GET")
            {
                respondChain (chain);
                return;
            }

            if (request.method === "POST")
            {
                readBody (body =>
                {
                    const insert = insertFromRequest (body, chain.inserts.length);
                    const position = typeof body.position === "number" ? body.position : -1;

                    if (position < 0 || position >= chain.inserts.length)
                        chain.inserts.push (insert);
                    else
                        chain.inserts.splice (position, 0, insert);

                    recalculateChain (chain);
                    fxRequests.push ({ method: "POST", slot, index: -1, body });
                    respondChain (chain);
                });

                return;
            }

            if (request.method === "PUT")
            {
                readBody (body =>
                {
                    const requested = Array.isArray (body.chain) ? body.chain : [];

                    chain.inserts = requested.map ((entry, index) =>
                        insertFromRequest (entry as Record<string, unknown>, index));
                    recalculateChain (chain);
                    fxRequests.push ({ method: "PUT", slot, index: -1, body });
                    respondChain (chain);
                });

                return;
            }
        }

        if (fxReorderMatch !== null && request.method === "POST")
        {
            const slot = Number.parseInt (fxReorderMatch[1] ?? "", 10);
            const chain = chainFor (slot);

            readBody (body =>
            {
                const order = Array.isArray (body.order) ? body.order as number[] : [];
                const reordered: ControlFxInsert[] = [];

                for (const index of order)
                {
                    const insert = chain.inserts[index];

                    if (insert !== undefined)
                        reordered.push (insert);
                }

                if (reordered.length !== chain.inserts.length)
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "order must be a permutation" }));
                    return;
                }

                chain.inserts = reordered;
                recalculateChain (chain);
                fxRequests.push ({ method: "POST", slot, index: -1, body });
                respondChain (chain);
            });

            return;
        }

        if (fxIndexMatch !== null)
        {
            const slot = Number.parseInt (fxIndexMatch[1] ?? "", 10);
            const index = Number.parseInt (fxIndexMatch[2] ?? "", 10);
            const chain = chainFor (slot);

            if (index < 0 || index >= chain.inserts.length)
            {
                response.statusCode = 404;
                response.end (JSON.stringify ({ error: "fx index is out of range" }));
                return;
            }

            if (request.method === "DELETE")
            {
                chain.inserts.splice (index, 1);
                recalculateChain (chain);
                fxRequests.push ({ method: "DELETE", slot, index, body: {} });
                respondChain (chain);
                return;
            }

            if (request.method === "PATCH")
            {
                readBody (body =>
                {
                    const insert = chain.inserts[index];

                    if (insert === undefined)
                    {
                        response.statusCode = 404;
                        response.end (JSON.stringify ({ error: "fx index is out of range" }));
                        return;
                    }

                    if (typeof body.bypassed === "boolean")
                        insert.bypassed = body.bypassed;

                    if (typeof body.wet === "number")
                    {
                        insert.wet = body.wet;
                        insert.dry = 1 - body.wet;
                    }

                    if (typeof body.position === "number"
                        && Number.isInteger (body.position)
                        && body.position >= 0
                        && body.position < chain.inserts.length
                        && body.position !== index)
                    {
                        chain.inserts.splice (index, 1);
                        chain.inserts.splice (body.position, 0, insert);
                    }

                    recalculateChain (chain);
                    fxRequests.push ({ method: "PATCH", slot, index, body });
                    respondChain (chain);
                });

                return;
            }
        }

        if (editorMatch !== null
            && (request.method === "POST" || request.method === "DELETE"))
        {
            const slot = Number.parseInt (editorMatch[1] ?? "", 10);
            const target = findPatch (slot);
            const open = request.method === "POST";

            editorRequests.push ({ slot, open });

            if (target === null)
            {
                response.statusCode = 404;
                response.end (JSON.stringify ({ error: "patch slot is out of range" }));
                return;
            }

            if (open && target.instrument.kind !== "plugin")
            {
                response.statusCode = 400;
                response.end (JSON.stringify ({
                    error: "the patch has no resident plugin to edit",
                }));
                return;
            }

            const wasOpen = target.editorOpen === true;
            target.editorOpen = open;
            response.statusCode = 200;
            response.end (JSON.stringify ({
                slot,
                open,
                ...(open ? { reused: wasOpen, wasOpen } : { wasOpen }),
            }));
            return;
        }

        if (stateCaptureMatch !== null && request.method === "POST")
        {
            const slot = Number.parseInt (stateCaptureMatch[1] ?? "", 10);
            const target = findPatch (slot);

            stateCaptureRequests.push ({ slot });

            if (target === null)
            {
                response.statusCode = 404;
                response.end (JSON.stringify ({ error: "patch slot is out of range" }));
                return;
            }

            if (target.instrument.kind !== "plugin")
            {
                response.statusCode = 400;
                response.end (JSON.stringify ({
                    error: "the patch has no resident plugin sound to save",
                }));
                return;
            }

            response.statusCode = 200;
            response.end (JSON.stringify ({
                slot,
                captured: true,
                sizeBytes: 4096 + slot,
            }));
            return;
        }

        if (sourceMatch !== null && request.method === "POST")
        {
            const slot = Number.parseInt (sourceMatch[1] ?? "", 10);
            const target = findPatch (slot);

            readBody (body =>
            {
                sourceRequests.push ({ slot, body });

                if (target === null)
                {
                    response.statusCode = 404;
                    response.end (JSON.stringify ({ error: "patch slot is out of range" }));
                    return;
                }

                if (typeof body.kind !== "string"
                    || ! ["empty", "init-tone", "sampler", "plugin", "audio-input"].includes (body.kind))
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "kind must be a source kind" }));
                    return;
                }

                const instrument: ControlState["families"][number]["patches"][number]["instrument"] = {
                    id: target.instrument.id,
                    name: typeof body.name === "string" && body.name.length > 0
                        ? body.name
                        : target.instrument.name,
                    kind: body.kind as ControlState["families"][number]["patches"][number]["instrument"]["kind"],
                };

                for (const key of ["pluginFormat", "pluginId", "pluginName",
                                   "pluginManufacturer", "pluginVersion",
                                   "samplePath"] as const)
                {
                    const value = body[key];

                    if (typeof value === "string" && value.length > 0)
                        Object.assign (instrument, { [key]: value });
                }

                target.instrument = instrument;
                target.editorOpen = false;

                response.statusCode = 200;
                response.end (JSON.stringify (state));
            });

            return;
        }

        if (request.method === "GET" && url.startsWith ("/plugins"))
        {
            pluginRequests.push (url);

            // A scan started by POST /plugins/rescan finishes before the next
            // read, exactly like a fast scan on the real inventory.
            if (pluginPendingScan)
            {
                pluginPendingScan = false;
                pluginScanning = false;

                if (! pluginPlugins.some (plugin => plugin.id === "com.instrumenthub.TestDelay"))
                {
                    pluginPlugins = [...pluginPlugins, {
                        id: "com.instrumenthub.TestDelay",
                        name: "TestDelay",
                        description: "Instrument Hub test delay",
                        vendor: "Instrument Hub",
                        version: "1.0.0",
                        category: "Fx",
                        isInstrument: false,
                        isEffect: true,
                        format: "VST3",
                        path: "/usr/lib/vst3/TestDelay.vst3",
                    }];
                }
            }

            const searchAt = url.indexOf ("search=");
            const search = searchAt < 0
                ? ""
                : decodeURIComponent (url.slice (searchAt + "search=".length).split ("&")[0] ?? "")
                    .toLowerCase();

            const plugins = pluginPlugins.filter (plugin =>
                search.length === 0
                || plugin.name.toLowerCase ().includes (search)
                || plugin.vendor.toLowerCase ().includes (search)
                || plugin.category.toLowerCase ().includes (search)
                || plugin.path.toLowerCase ().includes (search));

            response.statusCode = 200;
            response.end (JSON.stringify ({
                ...PLUGIN_FIXTURE,
                plugins,
                count: plugins.length,
                total: pluginPlugins.length,
                search,
                scanning: pluginScanning,
            }));
            return;
        }

        if (request.method === "POST" && url === "/plugins/rescan")
        {
            pluginRequests.push ("rescan");
            pluginScanning = true;
            pluginPendingScan = true;

            response.statusCode = 202;
            response.end (JSON.stringify ({
                ...PLUGIN_FIXTURE,
                plugins: pluginPlugins,
                count: pluginPlugins.length,
                total: pluginPlugins.length,
                search: "",
                scanning: true,
                started: true,
            }));
            return;
        }

        if (request.method === "GET" && url === "/midi")
        {
            response.statusCode = 200;
            response.end (JSON.stringify (midi));
            return;
        }

        if (request.method === "POST" && url === "/midi/inputs")
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                let parsed: { id?: unknown; enabled?: unknown };

                try
                {
                    parsed = JSON.parse (body) as { id?: unknown; enabled?: unknown };
                }
                catch
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "body must be a JSON object" }));
                    return;
                }

                if (typeof parsed.id !== "string" || typeof parsed.enabled !== "boolean")
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "id and enabled are required" }));
                    return;
                }

                const target = midi.inputs.find (input => input.id === parsed.id);

                if (target === undefined)
                {
                    response.statusCode = 404;
                    response.end (JSON.stringify ({ error: "unknown MIDI input" }));
                    return;
                }

                midiRequests.push ({ id: parsed.id, enabled: parsed.enabled });
                target.enabled = parsed.enabled;
                target.open = parsed.enabled;

                if (target.role === "launchpad")
                {
                    midi.launchpad.connected = parsed.enabled;
                    midi.launchpad.handshake = parsed.enabled;
                }

                if (target.role === "keystation")
                {
                    midi.keystation.connected = parsed.enabled;
                    midi.keystation.inputName = parsed.enabled ? target.name : "";
                }

                response.statusCode = 200;
                response.end (JSON.stringify (midi));
            });

            return;
        }

        if (request.method === "POST" && url === "/midi/pad-test")
        {
            padTestRequests += 1;
            midi.padTest.active = true;
            midi.padTest.lit = 64;

            response.statusCode = 200;
            response.end (JSON.stringify (midi));
            return;
        }

        if (request.method === "PATCH" && request.url?.startsWith ("/patches/") === true)
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                const slot = Number.parseInt (request.url?.slice ("/patches/".length) ?? "", 10);

                let parsed: Record<string, unknown>;

                try
                {
                    parsed = JSON.parse (body) as Record<string, unknown>;
                }
                catch
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "body must be a JSON object" }));
                    return;
                }

                patchRequests.push ({ slot, patch: parsed });

                if (! Number.isInteger (slot) || slot < 0)
                {
                    response.statusCode = 404;
                    response.end (JSON.stringify ({ error: "patch slot is out of range" }));
                    return;
                }

                let target: ControlState["families"][number]["patches"][number] | null = null;

                for (const family of state.families)
                    for (const patch of family.patches)
                        if (patch.slot === slot)
                            target = patch;

                if (target === null)
                {
                    response.statusCode = 404;
                    response.end (JSON.stringify ({ error: "patch slot is out of range" }));
                    return;
                }

                if (parsed.colorIndex !== undefined)
                {
                    const colour = parsed.colorIndex;

                    if (typeof colour !== "number" || ! Number.isInteger (colour)
                        || colour < 0 || colour > 15)
                    {
                        response.statusCode = 400;
                        response.end (JSON.stringify ({ error: "colorIndex must be 0..15" }));
                        return;
                    }

                    target.colorIndex = colour;
                }

                if (parsed.volumeDb !== undefined && typeof parsed.volumeDb === "number")
                    target.volumeDb = parsed.volumeDb;

                if (parsed.pan !== undefined && typeof parsed.pan === "number")
                    target.pan = parsed.pan;

                if (parsed.mute !== undefined && typeof parsed.mute === "boolean")
                    target.mute = parsed.mute;

                if (parsed.solo !== undefined && typeof parsed.solo === "boolean")
                    target.solo = parsed.solo;

                response.statusCode = 200;
                response.end (JSON.stringify (state));
            });

            return;
        }

        if (request.method === "GET" && request.url === "/state")
        {
            stateRequests += 1;

            if (stateRequests <= failFirst)
            {
                response.statusCode = 503;
                response.end (JSON.stringify ({ error: "engine is warming up" }));
                return;
            }

            response.statusCode = 200;
            response.end (JSON.stringify (state));
            return;
        }

        if (request.method === "POST" && request.url === "/arm")
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                let parsed: unknown;

                try
                {
                    parsed = JSON.parse (body);
                }
                catch
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "body must be a JSON object" }));
                    return;
                }

                const raw = (parsed as { slot?: unknown }).slot;
                const slot = typeof raw === "number" && Number.isInteger (raw) ? raw : null;

                if (raw !== undefined && raw !== null && slot === null)
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "slot must be an integer or null" }));
                    return;
                }

                armRequests.push ({ slot });

                const target = slot === null || slot < 0 ? -1 : slot;

                if (target >= state.slots.length)
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "slot is out of range" }));
                    return;
                }

                const apply = (): void =>
                {
                    state.armedSlot = target;
                    state.slots = state.slots.map (candidate => ({
                        ...candidate,
                        armed: candidate.index === target,
                    }));

                    response.statusCode = 200;
                    response.end (JSON.stringify (state));
                };

                if (armDelayMs > 0)
                    setTimeout (apply, armDelayMs);
                else
                    apply ();
            });

            return;
        }

        if (request.method === "POST" && request.url === "/transport")
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                let parsed: ControlTransportPatch;

                try
                {
                    parsed = JSON.parse (body) as ControlTransportPatch;
                }
                catch
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "body must be a JSON object" }));
                    return;
                }

                transportRequests.push (parsed);

                if (parsed.playing !== undefined)
                    state.transport.playing = parsed.playing;

                if (parsed.bpm !== undefined)
                    state.transport.bpm = parsed.bpm;

                if (parsed.ppq !== undefined)
                    state.transport.ppq = parsed.ppq;

                if (parsed.positionSamples !== undefined)
                    state.transport.positionSamples = parsed.positionSamples;

                response.statusCode = 200;
                response.end (JSON.stringify (state.transport));
            });

            return;
        }

        if (request.method === "GET" && request.url === "/audio")
        {
            response.statusCode = 200;
            response.end (JSON.stringify (audio));
            return;
        }

        if (request.method === "POST" && request.url === "/audio")
        {
            let body = "";

            request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });

            request.on ("end", () =>
            {
                let parsed: ControlAudioPatch;

                try
                {
                    parsed = JSON.parse (body) as ControlAudioPatch;
                }
                catch
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "body must be a JSON object" }));
                    return;
                }

                if (parsed.bufferSize !== undefined
                    && (! Number.isInteger (parsed.bufferSize)
                        || parsed.bufferSize < 16
                        || parsed.bufferSize > 8192))
                {
                    response.statusCode = 400;
                    response.end (JSON.stringify ({ error: "bufferSize must be between 16 and 8192" }));
                    return;
                }

                audioRequests.push (parsed);

                if (parsed.deviceType !== undefined)
                    audio.deviceType = parsed.deviceType;

                if (parsed.deviceName !== undefined)
                    audio.deviceName = parsed.deviceName;

                if (parsed.sampleRate !== undefined)
                    audio.sampleRate = parsed.sampleRate;

                if (parsed.bufferSize !== undefined)
                    audio.bufferSize = parsed.bufferSize;

                if (parsed.inputChannels !== undefined)
                {
                    const active = new Set (parsed.inputChannels);
                    audio.inputChannels = audio.inputChannels.map (channel => ({
                        ...channel,
                        active: active.has (channel.index),
                    }));
                }

                if (parsed.outputChannels !== undefined)
                {
                    const active = new Set (parsed.outputChannels);
                    audio.outputChannels = audio.outputChannels.map (channel => ({
                        ...channel,
                        active: active.has (channel.index),
                    }));
                }

                recalculateLatency (audio);

                response.statusCode = 200;
                response.end (JSON.stringify (audio));
            });

            return;
        }

        if (request.method === "POST" && request.url === "/audio/measure")
        {
            measureRequests += 1;

            const expectedSamples = audio.bufferSize * 2;
            const roundTripSamples = expectedSamples;
            const expectedMs = (expectedSamples * 1000) / audio.sampleRate;

            response.statusCode = 200;
            response.end (JSON.stringify ({
                valid: true,
                roundTripSamples,
                roundTripMs: (roundTripSamples * 1000) / audio.sampleRate,
                expectedMs,
                confidence: 0.94,
                sampleRate: audio.sampleRate,
                bufferSize: audio.bufferSize,
                error: "",
            }));
            return;
        }

        response.statusCode = 404;
        response.end (JSON.stringify ({ error: "not found" }));
    });

    const port = await listen (server);

    return {
        baseUrl: `http://127.0.0.1:${String (port)}`,
        get stateRequests () { return stateRequests; },
        get armRequests () { return armRequests; },
        get audioRequests () { return audioRequests; },
        get transportRequests () { return transportRequests; },
        get measureRequests () { return measureRequests; },
        get audio () { return audio; },
        get pairing () { return pairing; },
        get pairRequests () { return pairRequests; },
        get lanRequests () { return lanRequests; },
        get patchRequests () { return patchRequests; },
        get fxRequests () { return fxRequests; },
        get sourceRequests () { return sourceRequests; },
        get editorRequests () { return editorRequests; },
        get stateCaptureRequests () { return stateCaptureRequests; },
        get pluginRequests () { return pluginRequests; },
        get midiRequests () { return midiRequests; },
        get padTestRequests () { return padTestRequests; },
        get midi () { return midi; },
        get chain () { return chainFor (3); },
        close: () => new Promise ((resolve, reject) =>
        {
            server.close (error => { error === undefined ? resolve () : reject (error); });
        }),
    };
}
