// Headless snapshot test for the React UI.
//
// Runs the real app under `react-dom/server`: no browser, no Chrome, no
// WebView. The state is fetched over real HTTP from a fixture server that
// speaks the control API, then the rendered markup is asserted for the
// expected widget count and content and written to tests/artifacts.
//
// It also proves the grid-specific behaviour: 8 families × 8 patches, an
// optimistic arm highlight that moves on the next render (one frame, not one
// poll), per-slot meters fed from the WebSocket store, and the loading state.
//
// Run with: pnpm test:snapshot

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createServer, type IncomingMessage, type Server, type ServerResponse } from "node:http";
import type { AddressInfo } from "node:net";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import App from "../App";
import {
    ApiError,
    createControlApi,
    type ControlAudioMeasurement,
    type ControlAudioPatch,
    type ControlAudioState,
    type ControlPairingState,
    type ControlState,
    type ControlTransport,
    type ControlTransportPatch,
} from "../api/client";
import {
    AudioSettingsPanel,
    bufferSizePatch,
} from "../features/audio-settings";
import {
    codeFromSearch,
    encodeQr,
    loadPairingToken,
    PairingGate,
    qrPathData,
    type TokenStorage,
} from "../features/pairing";
import {
    createGridModel,
    EMPTY_METERS,
    GridView,
    MeterStore,
    type GridFamily,
    type MeterSnapshot,
    type MeterSocketEvent,
    type MeterSocketEventLike,
    type MeterSocketLike,
} from "../features/grid";
import {
    formatPosition,
    isRemotePath,
    RemoteView,
    remoteFromState,
    stepPatch,
} from "../features/remote";
import { AppStoreProvider } from "../state/AppStoreContext";
import { AppStore } from "../state/store";

const FIXTURE: ControlState = {
    schemaVersion: 1,
    application: { name: "Instrument Hub", version: "0.1.0" },
    armedSlot: 3,
    uptimeMs: 128_000,
    slots: [
        { index: 0, name: "Studio Grand", armed: false, latencySamples: 0, latencyMs: 0 },
        { index: 1, name: "Warm Pad", armed: false, latencySamples: 72, latencyMs: 1.5 },
        { index: 2, name: "Electric Keys", armed: false, latencySamples: 0, latencyMs: 0 },
        { index: 3, name: "Sub Bass", armed: true, latencySamples: 240, latencyMs: 5 },
        { index: 4, name: "Drum Kit", armed: false, latencySamples: 120, latencyMs: 2.5 },
        { index: 5, name: "Analog Brass", armed: false, latencySamples: 0, latencyMs: 0 },
        { index: 6, name: "Vocal Chop", armed: false, latencySamples: 96, latencyMs: 2 },
        { index: 7, name: "Empty Slot", armed: false, latencySamples: 0, latencyMs: 0 },
    ],
    settings: { masterGain: 0.82, inputGain: 1.0, midiChannel: 2, metronome: true },
    transport: { playing: true, bpm: 128, ppq: 16.5, positionSamples: 384_512 },
};

/** Audio device fixture for the settings panel and its live readout. */
const AUDIO: ControlAudioState = {
    open: true,
    deviceType: "ALSA",
    deviceName: "Loopback",
    deviceTypes: ["ALSA"],
    devices: [
        { type: "ALSA", name: "Loopback" },
        { type: "ALSA", name: "UR22mkII" },
    ],
    sampleRate: 48000,
    bufferSize: 128,
    sampleRates: [44100, 48000, 96000],
    bufferSizes: [64, 128, 256, 512],
    inputChannels: [
        { index: 0, name: "channel 1", active: true },
        { index: 1, name: "channel 2", active: true },
    ],
    outputChannels: [
        { index: 0, name: "channel 1", active: true },
        { index: 1, name: "channel 2", active: true },
    ],
    latency: {
        bufferMs: (128 * 1000) / 48000,
        deviceMs: 1.33,
        chainMs: 5,
        totalMs: (128 * 1000) / 48000 + 1.33 + 5,
    },
    xruns: 2,
    deviceXruns: 1,
    callbackOverruns: 1,
    callbacks: 128000,
    loadPercent: 7.5,
};

/**
    Pairing fixture. LAN mode starts off, exactly like a fresh engine.
*/
const PAIRING_OFF: ControlPairingState = {
    lanEnabled: false,
    address: "192.168.1.20",
    port: 8080,
    code: "",
    codeExpiresInMs: 0,
    payload: "",
    pairedClients: 0,
};

const PAIRING_ON: ControlPairingState = {
    lanEnabled: true,
    address: "192.168.1.20",
    port: 8080,
    code: "246810",
    codeExpiresInMs: 300_000,
    payload: "http://192.168.1.20:8080/remote?code=246810",
    pairedClients: 0,
};

/** Eight families for the feature-level grid snapshot. */const GRID_FAMILIES: GridFamily[] = [
    {
        id: "keys",
        name: "Keys",
        patches: [
            { id: "piano", name: "Grand Piano", slot: 0, latencySamples: 0, latencyMs: 0 },
            { id: "strings", name: "Warm Strings", slot: 1, latencySamples: 72, latencyMs: 1.5 },
        ],
    },
    {
        id: "drums",
        name: "Drums",
        patches: [{ id: "kit", name: "Acoustic Kit", slot: 2, latencySamples: 0, latencyMs: 0 }],
    },
    {
        id: "bass",
        name: "Bass",
        patches: [{ id: "sub", name: "Sub Bass", slot: 3, latencySamples: 240, latencyMs: 5 }],
    },
    {
        id: "pads",
        name: "Pads",
        patches: [{ id: "air", name: "Air Pad", slot: 4, latencySamples: 120, latencyMs: 2.5 }],
    },
    {
        id: "lead",
        name: "Lead",
        patches: [{ id: "saw", name: "Saw Lead", slot: 5, latencySamples: 48, latencyMs: 1 }],
    },
    {
        id: "fx",
        name: "FX",
        patches: [{ id: "riser", name: "Riser", slot: 6, latencySamples: 96, latencyMs: 2 }],
    },
    {
        id: "vocal",
        name: "Vocal",
        patches: [{ id: "chop", name: "Vocal Chop", slot: 7, latencySamples: 24, latencyMs: 0.5 }],
    },
    {
        id: "perc",
        name: "Percussion",
        patches: [{ id: "shaker", name: "Shaker", slot: null }],
    },
];

const GRID_MODEL = createGridModel (GRID_FAMILIES, 2);

const GRID_METERS: MeterSnapshot = {
    connected: true,
    sequence: 42,
    timestampMs: 1400.5,
    masterPeak: 0.31,
    slots: {
        0: { slot: 0, peak: 0.12, gain: 1, armed: false },
        1: { slot: 1, peak: 0.24, gain: 1, armed: false },
        2: { slot: 2, peak: 0.5, gain: 1, armed: true },
        3: { slot: 3, peak: 0.05, gain: 0.8, armed: false },
        4: { slot: 4, peak: 0, gain: 0.4, armed: false },
        5: { slot: 5, peak: 0.08, gain: 0.2, armed: false },
        6: { slot: 6, peak: 0, gain: 0, armed: false },
        7: { slot: 7, peak: 0, gain: 0, armed: false },
    },
};

interface FixtureServer
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
    close: () => Promise<void>;
}

interface FixtureOptions
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
async function startFixtureServer (options: FixtureOptions = {}): Promise<FixtureServer>
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
        close: () => new Promise ((resolve, reject) =>
        {
            server.close (error => { error === undefined ? resolve () : reject (error); });
        }),
    };
}

let failures = 0;

function check (condition: boolean, what: string): void
{
    console.log (`${condition ? "PASS" : "FAIL"} ${what}`);

    if (! condition)
        failures += 1;
}

function countOf (html: string, needle: string): number
{
    return html.split (needle).length - 1;
}

function artifactPath (name: string): string
{
    return fileURLToPath (new URL (`../../tests/artifacts/${name}`, import.meta.url));
}

function writeArtifact (name: string, html: string): void
{
    mkdirSync (fileURLToPath (new URL ("../../tests/artifacts/", import.meta.url)), { recursive: true });
    writeFileSync (artifactPath (name), html);

    console.log (`snapshot: wrote ${String (html.length)} bytes to ${artifactPath (name)}`);
}

function renderApp (store: AppStore): string
{
    return renderToStaticMarkup (
        <AppStoreProvider store={store}>
            <App />
        </AppStoreProvider>);
}

function waitFor (predicate: () => boolean, timeoutMs: number): Promise<void>
{
    return new Promise ((resolve, reject) =>
    {
        const deadline = Date.now () + timeoutMs;

        const poll = (): void =>
        {
            if (predicate ())
            {
                resolve ();
                return;
            }

            if (Date.now () > deadline)
            {
                reject (new Error (`condition not met within ${String (timeoutMs)} ms`));
                return;
            }

            setTimeout (poll, 5);
        };

        poll ();
    });
}

class MemoryStorage implements TokenStorage
{
    private readonly values = new Map<string, string> ();

    getItem (key: string): string | null
    {
        return this.values.get (key) ?? null;
    }

    setItem (key: string, value: string): void
    {
        this.values.set (key, value);
    }

    removeItem (key: string): void
    {
        this.values.delete (key);
    }
}

/**
    Reference matrix for

        http://instrument-hub.local:8080/remote?code=987654

    produced with `qrencode -t ASCII -v 4 -l M -m 0 -8` (mask 0, ECC M).
    Comparing against it proves the encoder's mode/count fields, Reed–Solomon
    blocks, interleaving, function patterns and format bits are all correct.
*/
const QR_GOLDEN = "111111100011100110111111001111111/100000101100110101010111101000001/101110100011000010011000001011101/101110100111010001010001101011101/101110101001100001000101101011101/100000100011010000100111101000001/111111101010101010101010101111111/000000000110010011001111100000000/101010100101111101011100000010010/011001001000001000001000111000001/111001111100110011000010111100111/011111010010100000011110000010001/101001101101100100001110101001010/110101001111110100010110101001001/010110100110001000001000100010011/110101000101101101011111100000010/011110100011010001000100011000000/110101011100011001001000111100011/101101101011100010101010110100101/000101010110110111110101101010010/111101110000101110110110111101001/010111000000010110100010111000011/100110100110110111000100010101111/011101011010100011111100111010001/101110110111001111001101111110001/000000001000101010001110100011001/111111100101101001000101101010101/100000100001110100011110100011010/101110101011101000010110111111000/101110100000000101000110100110101/101110101001101001101000110011101/100000100001000101101101110110010/111111101101011011001111110000011";

function qrTest (): void
{
    const golden = QR_GOLDEN.split ("/");
    const code = encodeQr ("http://instrument-hub.local:8080/remote?code=987654", { mask: 0 });

    check (code.size === 33 && code.version === 4, "QR is version 4 (33×33)");
    check (code.modules.map (row => row.map (value => value ? "1" : "0").join ("")).join ("/")
               === golden.join ("/"),
           "QR matrix matches the qrencode reference (mode, RS, interleave, format)");

    const path = qrPathData (code);
    check (path.length > 1000 && path.startsWith ("M0 0h1v1h-1z"),
           "QR path starts with the top-left finder module");

    const fixture = encodeQr (PAIRING_ON.payload);
    check (fixture.mask >= 0 && fixture.mask <= 7, "QR selects a valid mask");
    check (fixture.modules[0]!.slice (0, 7).every (Boolean)
               && fixture.modules[0]![7] === false,
           "top-left finder pattern is present");
    check (fixture.modules[fixture.size - 7]!.slice (0, 7).every (Boolean)
               && fixture.modules[fixture.size - 1]![0] === true,
           "bottom-left finder pattern is present");
    check (fixture.modules[0]!.slice (fixture.size - 7).every (Boolean),
           "top-right finder pattern is present");

    let tooLong = false;

    try
    {
        encodeQr ("x".repeat (63));
    }
    catch
    {
        tooLong = true;
    }

    check (tooLong, "a payload beyond version 4's capacity is rejected");

    check (codeFromSearch ("?code=123456") === "123456", "the QR link supplies the code");
    check (codeFromSearch ("?code=12x456") === null, "a malformed link code is ignored");
    check (codeFromSearch ("?other=1") === null, "a link without a code is ignored");
}

async function pairingTest (): Promise<void>
{
    const storage = new MemoryStorage ();
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { storage, pollIntervalMs: 60_000 });

        await store.refresh ();

        check (store.getSnapshot ().pairing?.lanEnabled === false,
               "the desktop reads LAN mode off from GET /pairing");
        check (store.token === null, "a fresh desktop has no pairing token");

        const before = renderApp (store);

        check (before.includes ('data-testid="pairing" data-lan="false"'),
               "pairing card renders with LAN mode off");
        check (! before.includes ('data-testid="pairing-qr"'),
               "no QR is rendered before LAN mode is enabled");

        const enabled = await store.toggleLan (true);

        check (enabled, "turning LAN mode on succeeds");
        check (store.getSnapshot ().pairing?.lanEnabled === true,
               "the store adopts the LAN-on pairing state");
        check (store.token === "fixture-pairing-token",
               "the desktop pairs itself with the issued code");
        check (server.lanRequests.at (-1) === true, "POST /pairing/lan received enabled: true");
        check (server.pairRequests.includes ("246810"),
               "the desktop redeemed the first six-digit code");
        check (loadPairingToken (storage) === "fixture-pairing-token",
               "the pairing token is stored client-side");

        const after = renderApp (store);

        check (after.includes ('data-testid="pairing" data-lan="true"'),
               "pairing card reports LAN mode on");
        check (after.includes ('data-testid="lan-toggle" aria-pressed="true"'),
               "the LAN toggle is pressed after enabling");
        check (after.includes ('data-testid="pairing-code" data-code="135790"'),
               "the rotated six-digit code reaches the panel");
        check (after.includes ("135 790"), "the code is readable in three-digit groups");
        check (after.includes ('data-testid="pairing-qr"')
                   && after.includes ('data-payload="http://192.168.1.20:8080/remote?code=135790"'),
               "the QR carries the live remote payload");
        check (after.includes ('data-version="4"'), "the QR identifies its version");
        check (after.includes ('data-testid="pairing-expiry">5:00<'),
               "the panel shows the code lifetime");
        check (after.includes ('data-testid="pairing-clients">1<'),
               "the panel counts the paired desktop");
        check (after.includes ('data-testid="pairing-token-state" data-paired="true"'),
               "the desktop reports itself as paired");

        // --- The acceptance flow, through a brand-new client --------------
        const anonymous = createControlApi ({ baseUrl: server.baseUrl });
        let unpairedStatus = 0;

        try
        {
            await anonymous.getState ();
        }
        catch (error)
        {
            unpairedStatus = error instanceof ApiError ? error.status : 0;
        }

        check (unpairedStatus === 401, "an unpaired client gets 401");

        let wrongCodeStatus = 0;

        try
        {
            await anonymous.pair ("000000");
        }
        catch (error)
        {
            wrongCodeStatus = error instanceof ApiError ? error.status : 0;
        }

        check (wrongCodeStatus === 401, "a wrong six-digit code is rejected");

        const paired = await anonymous.pair (server.pairing.code);

        check (paired.token === "fixture-pairing-token", "pairing with the code returns a token");

        anonymous.setToken (paired.token);

        const state = await anonymous.getState ();

        check (state.armedSlot === 3, "the token grants GET /state");

        // --- Tablet gate ---------------------------------------------------
        const gate = renderToStaticMarkup (
            <PairingGate pairing={server.pairing} initialCode="246810" onPair={() => {}} />);

        check (gate.includes ('data-testid="pairing-gate"'), "the remote gate renders");
        check (gate.includes ('data-testid="pairing-code-input"')
                   && gate.includes ('value="246810"'),
               "the gate pre-fills the scanned code");
        check (gate.includes ('data-testid="pairing-submit"'), "the gate has a pair button");
        check (gate.includes ("stays in this browser"), "the gate explains token storage");

        // --- Forget / turn LAN mode off -----------------------------------
        check (await store.forgetPairing (), "forgetting paired tablets succeeds");
        check (store.getSnapshot ().pairing?.pairedClients === 1,
               "forget revokes others and re-pairs this desktop");
        check (loadPairingToken (storage) === "fixture-pairing-token",
               "forgetting keeps this browser's fresh token");

        check (await store.toggleLan (false), "turning LAN mode off succeeds");
        check (store.getSnapshot ().pairing?.lanEnabled === false,
               "the store adopts the LAN-off pairing state");
        check (store.token === null, "turning LAN mode off drops the local token");
        check (loadPairingToken (storage) === null, "the stored token is removed");

        const off = renderApp (store);

        check (off.includes ('data-testid="pairing" data-lan="false"'),
               "the card returns to the LAN-off state");
        check (! off.includes ('data-testid="pairing-qr"'),
               "the QR disappears when LAN mode is off");
    }
    finally
    {
        await server.close ();
    }
}

async function snapshotTest (): Promise<void>
{
    const externalUrl = process.argv
        .find (argument => argument.startsWith ("--api="))
        ?.slice ("--api=".length);

    if (externalUrl !== undefined && externalUrl.length > 0)
    {
        console.log (`snapshot: rendering state from live API ${externalUrl}`);

        await renderSnapshot (externalUrl, false);

        return;
    }

    const server = await startFixtureServer ();

    try
    {
        await renderSnapshot (server.baseUrl, true);
        check (server.stateRequests === 1, "exactly one state request was made");
    }
    finally
    {
        await server.close ();
    }
}

async function renderSnapshot (baseUrl: string, fromFixture: boolean): Promise<void>
{
    const store = new AppStore (createControlApi ({ baseUrl }));
    const loaded = await store.refresh ();

    check (loaded, "GET /state succeeds over HTTP");

    const html = renderApp (store);

    writeArtifact (fromFixture ? "ui-state-snapshot.html" : "ui-state-live.html", html);

    check (html.length > 2000, "snapshot is non-blank");
    check (countOf (html, 'data-testid="family"') === 8, "snapshot shows 8 families");
    check (countOf (html, 'data-testid="grid-column"') === 8, "snapshot shows 8 patch columns");
    check (countOf (html, 'data-testid="grid-cell"') === 64, "snapshot shows a full 8×8 grid");
    check (countOf (html, 'data-testid="slot-meter"') === 8, "every slot has a level meter");
    check (countOf (html, 'data-testid="slot-latency"') === 8,
           "every fetched slot shows its chain latency");
    check (html.includes ('data-testid="slot-latency" data-slot="3" data-latency-samples="240" data-latency-ms="5.000"'),
           "the API chain latency reaches the grid");
    check (html.includes ('data-phase="ready"'), "connection is ready");
    check (html.includes ("Connected"), "snapshot shows the connected status");
    check (html.includes ("Instrument Hub"), "snapshot shows the application name");
    check (html.includes ("0.1.0"), "snapshot shows the application version");
    check (html.includes ('data-testid="armed-slot"'), "snapshot shows the armed-slot widget");
    check (html.includes ('data-testid="connection-status"'), "snapshot shows the connection widget");
    check (html.includes ('data-testid="transport-bpm"'), "snapshot shows the transport widget");
    check (html.includes ('data-testid="instrument-grid"'), "snapshot shows the instrument grid");
    check (html.includes ('data-testid="audio-settings"'), "snapshot shows the audio settings panel");
    check (html.includes ('data-testid="device-type-select"'), "panel shows the device type control");
    check (html.includes ('data-testid="device-select"'), "panel shows the device control");
    check (html.includes ('data-testid="sample-rate-select"'), "panel shows the sample rate control");
    check (html.includes ('data-testid="buffer-size-select"'), "panel shows the buffer size control");
    check (html.includes ('data-testid="latency-buffer"'), "panel shows the buffer latency");
    check (html.includes ('data-testid="latency-device"'), "panel shows the device latency");
    check (html.includes ('data-testid="latency-chain"'), "panel shows the chain latency");
    check (html.includes ('data-testid="latency-total"'), "panel shows the total latency");
    check (html.includes ('data-testid="xrun-count"'), "panel shows the xrun counter");
    check (html.includes ('data-testid="measure-button"'), "panel shows the Measure button");
    check (html.includes ('data-open="true"'), "panel reports the device is open");
    check (html.includes ('data-testid="pairing"'), "desktop shows the tablet pairing card");
    check (html.includes ('data-testid="pairing" data-lan="false"'),
           "LAN mode is off by default");
    check (html.includes ('data-testid="lan-toggle" aria-pressed="false"'),
           "the LAN toggle starts off");
    check (! html.includes ('data-testid="pairing-qr"'),
           "no QR code is rendered while LAN mode is off");
    check (html.includes ('data-testid="pairing-token-state" data-paired="false"'),
           "an unpaired desktop reports no token");
    check (countOf (html, 'data-testid="output-channel"')
               === (store.getSnapshot ().audio?.outputChannels.length ?? 0),
           "panel maps every output channel the engine reports");

    // The markup must agree with the state the API actually returned.
    const armedSlot = store.getSnapshot ().state?.armedSlot ?? -1;
    check (countOf (html, 'data-armed="true"') === (armedSlot >= 0 ? 1 : 0),
           `armed-slot highlight matches the fetched state (armedSlot ${String (armedSlot)})`);
    check ((armedSlot < 0) === html.includes ("armed none"),
           "footer reports the fetched arm state");

    if (fromFixture)
    {
        check (countOf (html, 'data-armed="true"') === 1, "exactly one cell is armed");
        check (html.includes ('data-slot="3" data-armed="true"'), "the armed cell is slot 3");
        check (html.includes ("Sub Bass"), "snapshot shows the armed slot name");
        check (html.includes ("Studio Grand"), "snapshot shows the first slot name");
        check (html.includes ("128 BPM"), "snapshot shows the fetched tempo");
        check (html.includes ("Playing"), "snapshot shows the fetched transport state");
    }
}

async function gridSnapshotTest (): Promise<void>
{
    const html = renderToStaticMarkup (
        <GridView model={GRID_MODEL} meters={GRID_METERS} onArm={() => {}} />);

    writeArtifact ("ui-grid-snapshot.html", html);

    check (html.length > 2000, "grid snapshot is non-blank");
    check (countOf (html, 'data-testid="family"') === 8, "grid snapshot shows 8 families");
    check (countOf (html, 'data-testid="grid-column"') === 8, "grid snapshot shows 8 patch columns");
    check (countOf (html, 'data-testid="grid-cell"') === 64, "grid snapshot is 8×8 capable");
    check (countOf (html, 'data-empty="true"') === 55, "unused patches render as disabled cells");
    check (countOf (html, 'data-testid="slot-meter"') === 9, "each assigned patch owns a meter");
    check (countOf (html, 'data-testid="slot-latency"') === 8,
           "each assigned patch shows its chain latency");
    check (html.includes ('data-testid="slot-latency" data-slot="3" data-latency-samples="240" data-latency-ms="5.000"'),
           "the 5 ms chain latency is published into the grid markup");
    check (html.includes (">5.00 ms<"), "the grid renders the latency in milliseconds");
    check (html.includes (">Keys<"), "grid names the first family");
    check (html.includes ("Grand Piano") && html.includes ("Warm Strings"),
           "a family renders more than one patch");
    check (countOf (html, 'data-armed="true"') === 1, "exactly one grid cell is armed");
    check (html.includes ('data-slot="2" data-armed="true"'), "the armed cell is highlighted");
    check (html.includes ('data-testid="slot-meter" data-slot="2" data-peak="0.500"'),
           "per-slot meter carries the latest peak");
    check (html.includes ('data-testid="master-meter" data-peak="0.310"'),
           "master meter carries the latest peak");
    check (html.includes ('data-testid="meters-status" data-connected="true"'),
           "grid reports live meters");
    check (html.includes ('data-slot="-1" data-armed="false"'),
           "an unassigned patch still renders a cell");

    const loadingHtml = renderToStaticMarkup (
        <GridView model={createGridModel ([], -1)} meters={EMPTY_METERS} loading />);

    writeArtifact ("ui-grid-loading.html", loadingHtml);

    check (loadingHtml.includes ('data-testid="grid-loading"'), "loading state renders a skeleton");
    check (loadingHtml.includes ('aria-busy="true"'), "loading grid is marked busy");
    check (countOf (loadingHtml, 'data-testid="grid-skeleton-cell"') === 64,
           "loading skeleton keeps the 8×8 layout");
    check (countOf (loadingHtml, 'data-testid="family"') === 0,
           "loading state does not show stale families");

    const css = readFileSync (
        fileURLToPath (new URL ("../src/features/grid/grid.css", import.meta.url)), "utf8");

    check (/min-width:\s*44px/.test (css) && /touch-action:\s*manipulation/.test (css),
           "grid cells are touch-friendly (44 px targets, manipulation gestures)");
}

async function armTest (): Promise<void>
{
    const server = await startFixtureServer ({ armDelayMs: 250 });

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        await store.refresh ();

        const before = renderApp (store);
        check (countOf (before, 'data-armed="true"') === 1, "one cell is armed before arming");
        check (before.includes ('data-slot="3" data-armed="true"'), "initial highlight is on slot 3");

        const startedAt = Date.now ();
        const pendingArm = store.armSlot (5);
        const immediate = store.getSnapshot ();

        check (immediate.state?.armedSlot === 5, "arm updates the store synchronously");
        check (immediate.state?.slots.filter (slot => slot.armed).length === 1,
               "optimistic state keeps exactly one armed slot");

        const after = renderApp (store);
        check (countOf (after, 'data-armed="true"') === 1,
               "exactly one cell stays highlighted after arming");
        check (after.includes ('data-slot="5" data-armed="true"'),
               "the newly armed cell is highlighted on the next render");
        check (! after.includes ('data-slot="3" data-armed="true"'),
               "the previously armed cell lost its highlight");
        check (Date.now () - startedAt < 200,
               "the highlight moved before the delayed POST /arm completed");

        const ok = await pendingArm;
        check (ok, "POST /arm resolves successfully");
        check (server.armRequests.length === 1 && server.armRequests[0]?.slot === 5,
               "the API received slot 5");
        check (store.getSnapshot ().state?.armedSlot === 5, "store keeps the confirmed armed slot");
        check (countOf (renderApp (store), 'data-armed="true"') === 1,
               "highlight survives the confirmation");

        const requestsBefore = server.armRequests.length;
        const rejected = await store.armSlot (99);

        check (! rejected, "an out-of-range slot is rejected");
        check (server.armRequests.length === requestsBefore,
               "an out-of-range slot never reaches the API");
        check (store.getSnapshot ().state?.armedSlot === 5,
               "a rejected arm leaves the highlight alone");
    }
    finally
    {
        await server.close ();
    }
}

class FakeMeterSocket implements MeterSocketLike
{
    readonly listeners = new Map<MeterSocketEvent, ((event: MeterSocketEventLike) => void)[]> ();
    closed = false;

    addEventListener (type: MeterSocketEvent,
                      listener: (event: MeterSocketEventLike) => void): void
    {
        const existing = this.listeners.get (type) ?? [];
        existing.push (listener);
        this.listeners.set (type, existing);
    }

    close (): void
    {
        this.closed = true;
        this.emit ("close", {});
    }

    emit (type: MeterSocketEvent, event: MeterSocketEventLike): void
    {
        for (const listener of this.listeners.get (type) ?? [])
            listener (event);
    }
}

function meterTest (): void
{
    const socket = new FakeMeterSocket ();
    const store = new MeterStore ({
        url: "ws://127.0.0.1:9/meters",
        socketFactory: () => socket,
        reconnectBaseMs: 10_000,
        reconnectMaxMs: 10_000,
    });

    store.start ();
    socket.emit ("open", {});

    check (store.getSnapshot ().connected, "meter socket reports connected");

    socket.emit ("message", {
        data: JSON.stringify ({
            type: "meters",
            sequence: 7,
            timestampMs: 100.5,
            master: { peak: 0.4 },
            slots: [
                { slot: 2, peak: 0.5, gain: 1.0, armed: false },
                { slot: 3, peak: 0.25, gain: 0.75, armed: true },
            ],
        }),
    });

    check (store.getSnapshot ().slots[2]?.peak === 0.5, "per-slot peak is stored");
    check (store.getSnapshot ().masterPeak === 0.4, "master peak is stored");
    check (store.getSnapshot ().sequence === 7, "frame sequence is stored");
    check (! store.ingest ({ type: "meters" }), "a frame without slots is rejected");
    check (! store.ingest ("not json"), "a malformed frame is ignored");

    const metersHtml = renderToStaticMarkup (
        <GridView model={GRID_MODEL} meters={store.getSnapshot ()} onArm={() => {}} />);

    check (metersHtml.includes ('data-testid="slot-meter" data-slot="2" data-peak="0.500"'),
           "the grid renders the live slot meter");

    store.stop ();

    check (socket.closed, "stop closes the meter socket");
    check (! store.getSnapshot ().connected, "stopped meters report disconnected");
}

function loadingTest (): void
{
    const store = new AppStore (createControlApi ({ baseUrl: "http://127.0.0.1:1" }));
    const html = renderApp (store);

    check (html.includes ('data-testid="grid-loading"'),
           "grid shows a loading skeleton before the first state");
    check (countOf (html, 'data-testid="grid-skeleton-cell"') === 64,
           "loading skeleton is 8×8");
    check (html.includes ('aria-busy="true"'), "loading grid is marked busy");
}

async function reconnectTest (): Promise<void>
{
    const server = await startFixtureServer ({ failFirst: 2 });

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { retryBaseMs: 5, retryMaxMs: 20, pollIntervalMs: 60_000 });

        store.start ();

        try
        {
            await waitFor (() => store.getSnapshot ().phase === "ready", 3000);
        }
        finally
        {
            store.stop ();
        }

        const snapshot = store.getSnapshot ();

        check (snapshot.phase === "ready", "store reconnects after HTTP failures");
        check (snapshot.state?.armedSlot === 3, "reconnected store holds the fetched state");
        check (snapshot.failedAttempts === 0, "failure counter resets after a success");
        check (server.stateRequests >= 3, "reconnect retried the request");
    }
    finally
    {
        await server.close ();
    }
}

async function backoffTest (): Promise<void>
{
    const failingApi = {
        baseUrl: "http://127.0.0.1:1",
        token: null as string | null,
        setToken (): void {},
        getState: async (): Promise<ControlState> =>
        {
            throw new Error ("connection refused");
        },
        armSlot: async (): Promise<ControlState> =>
        {
            throw new Error ("connection refused");
        },
        controlTransport: async (): Promise<ControlTransport> =>
        {
            throw new Error ("connection refused");
        },
        getAudio: async (): Promise<ControlAudioState> =>
        {
            throw new Error ("connection refused");
        },
        updateAudio: async (): Promise<ControlAudioState> =>
        {
            throw new Error ("connection refused");
        },
        measureLatency: async (): Promise<ControlAudioMeasurement> =>
        {
            throw new Error ("connection refused");
        },
        getPairing: async () =>
        {
            throw new Error ("connection refused");
        },
        setLanMode: async () =>
        {
            throw new Error ("connection refused");
        },
        pair: async () =>
        {
            throw new Error ("connection refused");
        },
        revokePairing: async () =>
        {
            throw new Error ("connection refused");
        },
    };

    const store = new AppStore (failingApi, { retryBaseMs: 5, retryMaxMs: 15 });

    await store.refresh ();
    check (store.retryDelayMs () === 5, "first retry waits for the base delay");

    await store.refresh ();
    check (store.retryDelayMs () === 10, "second retry doubles the delay");

    await store.refresh ();
    check (store.retryDelayMs () === 15, "retry delay is capped");

    check (store.getSnapshot ().phase === "offline", "store reports offline after failures");
    check (store.getSnapshot ().error === "connection refused", "store keeps the failure message");
}

async function audioSettingsTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        const loaded = await store.refresh ();
        check (loaded, "audio fixture loads with GET /state + GET /audio");
        check (store.getSnapshot ().audio?.bufferSize === 128,
               "store holds the fetched 128-frame buffer");
        check (store.getSnapshot ().audio?.latency.bufferMs === (128 * 1000) / 48000,
               "fetched buffer latency is one buffer at 48 kHz");

        const initial = renderApp (store);

        check (initial.includes ('data-buffer-size="128"'),
               "panel renders the fetched buffer size");
        check (initial.includes ('data-testid="latency-buffer">2.67 ms'),
               "buffer latency readout shows 2.67 ms at 128 frames / 48 kHz");
        check (initial.includes ('data-testid="latency-chain">5.00 ms'),
               "chain latency readout shows the summed chain total");
        check (initial.includes ('data-testid="latency-total">9.00 ms'),
               "total latency is buffer + device + chain");
        check (initial.includes ('data-testid="xrun-count">2<'),
               "xrun counter shows the device + callback total");

        const changed = await store.updateAudio (bufferSizePatch (256));

        check (changed, "changing the buffer size succeeds");
        check (server.audioRequests.length === 1
                   && server.audioRequests[0]?.bufferSize === 256,
               "the UI sent { bufferSize: 256 } to POST /audio");
        check (server.audio.bufferSize === 256,
               "the engine fixture applied 256 frames");
        check (store.getSnapshot ().audio?.bufferSize === 256,
               "store adopts the post-change buffer size");

        const after = renderApp (store);

        check (after.includes ('data-buffer-size="256"'),
               "the readout updates to 256 frames");
        check (after.includes ('data-testid="latency-buffer">5.33 ms'),
               "buffer latency updates to 5.33 ms at 256 frames / 48 kHz");
        check (after.includes ('data-testid="latency-total">11.7 ms'),
               "total latency readout updates with the buffer size");
        check (after.includes ("256 frames"),
               "the buffer control lists the new current value");

        const measured = await store.measureLatency ();
        const measurement = store.getSnapshot ().measurement;

        check (measured && measurement?.valid === true,
               "Measure returns a valid loopback measurement");
        check (server.measureRequests === 1,
               "the Measure button path called POST /audio/measure");
        check (measurement !== null && Math.abs (measurement.roundTripMs - 10.6667) < 0.1,
               "measured round trip is two buffers at 256 frames (10.7 ms)");

        const measuredHtml = renderApp (store);

        check (measuredHtml.includes ('data-testid="measure-result" data-valid="true"'),
               "the panel renders the successful measurement");
        check (measuredHtml.includes ("10.7 ms"),
               "the panel shows the measured round-trip in milliseconds");
        check (measuredHtml.includes ("512 samples"),
               "the panel shows the measured sample count");

        const remapped = await store.updateAudio ({ outputChannels: [0] });

        check (remapped && store.getSnapshot ().audio?.outputChannels[1]?.active === false,
               "channel mapping changes reach the engine and come back");

        const rejected = await store.updateAudio ({ bufferSize: 0 });

        check (! rejected, "an invalid buffer size is rejected");
        check (store.getSnapshot ().audioError?.includes ("bufferSize") === true,
               "the panel surfaces the engine rejection");

        const loadingHtml = renderToStaticMarkup (
            <AudioSettingsPanel audio={null} loading />);

        check (loadingHtml.includes ('data-testid="audio-settings-loading"'),
               "audio panel shows a loading state before the first GET /audio");
        check (loadingHtml.includes ('aria-busy="true"'),
               "loading audio panel is marked busy");
    }
    finally
    {
        await server.close ();
    }
}

function remoteSnapshotTest (): void
{
    const model = remoteFromState (FIXTURE);
    const html = renderToStaticMarkup (
        <RemoteView model={model} connection="ready" />);

    writeArtifact ("ui-remote-snapshot.html", html);

    check (html.length > 2000, "remote snapshot is non-blank");
    check (html.includes ('data-testid="remote"'), "remote renders its root");
    check (html.includes ('data-testid="remote-set-count">8 patches<'),
           "remote set list reports eight patches");
    check (countOf (html, 'data-testid="remote-set-item"') === 8,
           "remote renders one large target per patch");
    check (countOf (html, 'data-armed="true"') === 1, "exactly one remote patch is armed");
    check (html.includes ('data-testid="remote-set-item" data-slot="3" data-armed="true"'),
           "the remote highlights the armed slot");
    check (html.includes ('data-testid="remote-current-name">Sub Bass<'),
           "the now-playing card shows the armed patch");
    check (html.includes ("chain 5.0 ms"), "the now-playing card shows chain latency");
    check (html.includes ('data-testid="remote-prev"'), "remote has a previous-patch control");
    check (html.includes ('data-testid="remote-next"'), "remote has a next-patch control");
    check (html.includes ('data-testid="remote-play"'), "remote has a play/pause control");
    check (html.includes ('data-testid="remote-stop"'), "remote has a transport stop control");
    check (html.includes ('data-testid="remote-panic"'), "remote has a panic control");
    check (html.includes ('data-testid="remote-bpm">128 BPM<'),
           "remote shows the fetched tempo");
    check (html.includes ('data-testid="remote-transport-state">Playing<'),
           "remote shows the fetched transport state");
    check (html.includes ('data-playing="true"'), "remote root reports playing state");
    check (html.includes ('data-testid="remote-position">Bar 5.1'),
           "remote shows the musical position");

    const css = readFileSync (
        fileURLToPath (new URL ("../src/features/remote/remote.css", import.meta.url)), "utf8");

    check (/\.remote button\s*\{[^}]*min-height:\s*44px/s.test (css),
           "every remote button has a 44 px minimum height");
    check (/button\.remote__set-item\s*\{[^}]*min-height:\s*56px/s.test (css),
           "set-list rows are larger than the 44 px minimum");
    check (/button\.remote__step-button\s*\{[^}]*min-height:\s*clamp\(56px/s.test (css),
           "previous/next buttons are at least 56 px high");
    check (/button\.remote__panic-button\s*\{[^}]*min-height:\s*64px/s.test (css),
           "panic is a 64 px target");
    check (/overflow-x:\s*clip/.test (css), "the remote clamps horizontal overflow");
    check (/grid-template-columns:\s*minmax\(0, 1\.5fr\) minmax\(0, 1fr\)/.test (css),
           "the 1024 px layout uses two shrinkable columns");
    check (/@media \(max-width: 820px\)/.test (css), "the remote is responsive below 820 px");
    check (/(width|min-width):\s*(?:[5-9]\d|\d{3,})rem/.test (css) === false,
           "no fixed layout width exceeds the viewport");

    const minWidths = [...css.matchAll (/min-width:\s*(\d+)px/g)]
        .map (match => Number (match[1]));

    check (minWidths.length > 0 && minWidths.every (value => value === 0 || value >= 44),
           "every pixel min-width is either zero or a 44 px+ touch target");

    const loadingHtml = renderToStaticMarkup (
        <RemoteView model={remoteFromState (null)} connection="connecting" loading />);

    check (loadingHtml.includes ('aria-busy="true"'), "remote loading state is marked busy");
    check (loadingHtml.includes ("Loading the set"), "remote loading state names the set");
}

function remoteModelTest (): void
{
    const model = remoteFromState (FIXTURE);

    check (model.patches.length === 8, "remote model keeps every set-list patch");
    check (model.armedSlot === 3, "remote model keeps the armed slot");
    check (stepPatch (model, 1) === 4, "next moves one patch forward in set order");
    check (stepPatch (model, -1) === 2, "previous moves one patch back in set order");

    const last = { ...model, armedSlot: 7 };
    const first = { ...model, armedSlot: 0 };
    const none = { ...model, armedSlot: -1 };

    check (stepPatch (last, 1) === 0, "next wraps from the last patch to the first");
    check (stepPatch (first, -1) === 7, "previous wraps from the first patch to the last");
    check (stepPatch (none, 1) === 0, "next from a disarmed set starts at the first patch");
    check (stepPatch (none, -1) === 7, "previous from a disarmed set starts at the last patch");

    const sparse = {
        ...model,
        patches: [
            { id: "slot-1", name: "One", slot: 1, latencyMs: 0 },
            { id: "slot-5", name: "Five", slot: 5, latencyMs: 0 },
            { id: "slot-9", name: "Nine", slot: 9, latencyMs: 0 },
        ],
        armedSlot: 5,
    };

    check (stepPatch (sparse, 1) === 9 && stepPatch (sparse, -1) === 1,
           "next/previous follow set order across non-contiguous slot numbers");
    check (stepPatch ({ ...model, patches: [] }, 1) === null,
           "stepping an empty set produces no slot");
    check (formatPosition (16.5) === "5.1", "position 16.5 PPQ is bar 5 beat 1");
    check (isRemotePath ("/remote") && isRemotePath ("/remote/")
               && ! isRemotePath ("/") && ! isRemotePath ("/remote/extra"),
           "only the /remote path selects the remote route");
}

async function remoteInteractionTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        await store.refresh ();

        const initial = remoteFromState (store.getSnapshot ().state);
        const next = stepPatch (initial, 1);

        check (next === 4, "remote Next resolves to slot 4");
        check (await store.armSlot (next), "remote Next arms the patch through POST /arm");

        const afterNext = renderToStaticMarkup (
            <RemoteView model={remoteFromState (store.getSnapshot ().state)} connection="ready" />);

        check (afterNext.includes ('data-testid="remote-set-item" data-slot="4" data-armed="true"'),
               "the remote highlight follows Next");
        check (afterNext.includes ('data-testid="remote-current-name">Drum Kit<'),
               "the now-playing card follows Next");

        check (await store.updateTransport ({ playing: false }),
               "remote transport control reaches POST /transport");
        check (server.transportRequests.length === 1
                   && server.transportRequests[0]?.playing === false,
               "the engine received playing: false");

        const paused = renderToStaticMarkup (
            <RemoteView model={remoteFromState (store.getSnapshot ().state)} connection="ready" />);

        check (paused.includes ('data-testid="remote-transport-state">Stopped<'),
               "the remote renders the paused transport");
        check (paused.includes ('data-testid="remote-play"') && paused.includes (">Play<"),
               "the play button flips to Play");

        check (await store.panic (), "panic stops the transport and disarms the set");
        check (store.getSnapshot ().state?.armedSlot === -1, "panic leaves no armed slot");
        check (server.armRequests.at (-1)?.slot === null, "panic sent a disarm request");
        check (server.transportRequests.at (-1)?.playing === false,
               "panic stopped the transport");
        check (server.transportRequests.at (-1)?.ppq === 0,
               "panic rewound the transport");

        const panicked = renderToStaticMarkup (
            <RemoteView model={remoteFromState (store.getSnapshot ().state)} connection="ready" />);

        check (panicked.includes ('data-testid="remote-current-name">Nothing armed<'),
               "the remote shows nothing armed after panic");
        check (countOf (panicked, 'data-armed="true"') === 0,
               "no set-list row stays highlighted after panic");
    }
    finally
    {
        await server.close ();
    }
}

async function main (): Promise<void>
{
    await snapshotTest ();
    await gridSnapshotTest ();
    await armTest ();
    meterTest ();
    loadingTest ();
    await audioSettingsTest ();
    await pairingTest ();
    qrTest ();
    remoteSnapshotTest ();
    remoteModelTest ();
    await remoteInteractionTest ();
    await reconnectTest ();
    await backoffTest ();

    if (failures > 0)
    {
        console.error (`ui-snapshot: ${String (failures)} check(s) failed`);
        process.exitCode = 1;
        return;
    }

    console.log ("ui-snapshot: all checks passed");
}

main ().catch (error =>
{
    console.error ("ui-snapshot: ", error);
    process.exitCode = 1;
});
