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

import App, { armAndOpenInstrument } from "../App";
import {
    ApiError,
    createControlApi,
    type ControlAudioMeasurement,
    type ControlAudioPatch,
    type ControlAudioState,
    type ControlFxChain,
    type ControlFxInsert,
    type ControlMidiState,
    type ControlPairingState,
    type ControlPatch,
    type ControlPlugin,
    type ControlPluginList,
    type ControlState,
    type ControlTransport,
    type ControlTransportPatch,
} from "../api/client";
import {
    AudioSettingsPanel,
    bufferSizePatch,
} from "../features/audio-settings";
import {
    chainWithBypass,
    chainWithMove,
    chainWithoutInsert,
    clampPan,
    effectPlugins,
    FADER_MIN_DB,
    followsArmedSlot,
    formatDb,
    formatPan,
    gainFromDb,
    InstrumentPage,
    instrumentPlugins,
    instrumentSlotForRoute,
    insertRequestForBuiltin,
    insertRequestForPlugin,
    isPluginSource,
    moveOrder,
    sourceKindLabel,
    sourceRequestForInitTone,
    sourceRequestForPlugin,
    wetPercent,
} from "../features/instrument";
import {
    filterPlugins,
    formatScanTime,
    pluginAssignments,
    pluginAssignmentsFromState,
    pluginCountLabel,
    PluginsPage,
    pluginKind,
    usedByPlugin,
} from "../features/plugins";
import {
    canRunPadTest,
    isLaunchpadDaInput,
    MidiPanel,
    midiInputForRole,
    midiInputState,
    midiInputSummary,
    midiPortLabel,
    midiPortState,
    midiRoleLabel,
    padTestLabel,
} from "../features/midi";
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
    PATCH_COLOURS,
    type GridFamily,
    type MeterSnapshot,
    type MeterSocketEvent,
    type MeterSocketEventLike,
    type MeterSocketLike,
} from "../features/grid";
import {
    DEFAULT_REMOTE_CONTRAST,
    formatPosition,
    INITIAL_LINK,
    isRemotePath,
    loadRemoteContrast,
    MeterLink,
    nextContrast,
    RemoteView,
    remoteBanner,
    remoteFromState,
    saveRemoteContrast,
    stepPatch,
    WakeLockController,
    type RemoteLinkSnapshot,
    type WakeLockCapableNavigator,
    type WakeLockSentinelLike,
    type WakeLockStatus,
} from "../features/remote";
import { AppStoreProvider } from "../state/AppStoreContext";
import {
    instrumentPathForSlot,
    pathForRoute,
    routeFromPath,
    SHELL_ROUTES,
    slotFromSearch,
} from "../state/router";
import { AppStore } from "../state/store";

type FixturePatch = ControlState["families"][number]["patches"][number];

const FIXTURE_FAMILIES: readonly { id: string; name: string; patches: readonly [string, string][] }[] = [
    {
        id: "keys",
        name: "Keys",
        patches: [
            ["piano", "Studio Grand"],
            ["strings", "Warm Pad"],
            ["rhodes", "Electric Keys"],
            ["organ", "Drawbar Organ"],
            ["harp", "Concert Harp"],
            ["choir", "Choir Keys"],
            ["celesta", "Celesta"],
            ["clav", "Clavinet"],
        ],
    },
    {
        id: "drums",
        name: "Drums",
        patches: [
            ["kit", "Acoustic Kit"],
            ["snare", "Snare Stack"],
            ["hihats", "Hi-Hats"],
            ["toms", "Toms"],
            ["clap", "Clap"],
            ["rimshot", "Rimshot"],
            ["cymbals", "Cymbals"],
            ["kick", "Kick"],
        ],
    },
    {
        id: "bass",
        name: "Bass",
        patches: [
            ["sub", "Sub Bass"],
            ["finger", "Finger Bass"],
            ["pick", "Pick Bass"],
            ["fretless", "Fretless Bass"],
            ["synth", "Synth Bass"],
            ["upright", "Upright Bass"],
            ["growl", "Growl Bass"],
            ["octave", "Octave Bass"],
        ],
    },
    {
        id: "pads",
        name: "Pads",
        patches: [
            ["air", "Air Pad"],
            ["warm", "Warm Pad"],
            ["glass", "Glass Pad"],
            ["choir", "Choir Pad"],
            ["motion", "Motion Pad"],
            ["dark", "Dark Pad"],
            ["shimmer", "Shimmer"],
            ["tape", "Tape Pad"],
        ],
    },
    {
        id: "lead",
        name: "Lead",
        patches: [
            ["saw", "Saw Lead"],
            ["square", "Square Lead"],
            ["sine", "Sine Lead"],
            ["sync", "Sync Lead"],
            ["fifth", "Fifth Lead"],
            ["soft", "Soft Lead"],
            ["hard", "Hard Lead"],
            ["pulse", "Pulse Lead"],
        ],
    },
    {
        id: "fx",
        name: "FX",
        patches: [
            ["riser", "Riser"],
            ["down", "Downlifter"],
            ["impact", "Impact"],
            ["noise", "Noise Sweep"],
            ["sweep", "Sweep"],
            ["reverse", "Reverse"],
            ["texture", "Texture"],
            ["drone", "Drone"],
        ],
    },
    {
        id: "perc",
        name: "Percussion",
        patches: [
            ["shaker", "Shaker"],
            ["tamb", "Tambourine"],
            ["conga", "Conga"],
            ["bongo", "Bongo"],
            ["cowbell", "Cowbell"],
            ["clave", "Clave"],
            ["woodblock", "Woodblock"],
            ["triangle", "Triangle"],
        ],
    },
    {
        id: "vox",
        name: "Vox",
        patches: [
            ["chop", "Vocal Chop"],
            ["ooh", "Vocal Ooh"],
            ["aah", "Vocal Aah"],
            ["stack", "Vocal Stack"],
            ["phrase", "Vocal Phrase"],
            ["whisper", "Whisper"],
            ["shout", "Shout"],
            ["robot", "Robot Vox"],
        ],
    },
];

let fixtureSlot = 0;

const FIXTURE_PATCHES: ControlState["families"] = FIXTURE_FAMILIES.map (
    (family, familyIndex) => ({
        id: family.id,
        name: family.name,
        patches: family.patches.map (([id, name]): FixturePatch =>
        {
            const slot = fixtureSlot;
            fixtureSlot += 1;

            return {
                slot,
                id,
                name,
                family: family.id,
                familyName: family.name,
                instrument: slot === 6
                    ? {
                          id: "com.arturia.AnalogLab",
                          name: "Analog Lab V",
                          kind: "plugin",
                          pluginFormat: "VST3",
                          pluginId: "com.arturia.AnalogLab",
                          pluginName: "Analog Lab V",
                          pluginManufacturer: "Arturia",
                          pluginVersion: "5.10.0",
                      }
                    : { id, name, kind: "init-tone" },
                volumeDb: 0,
                pan: 0,
                mute: false,
                solo: false,
                colorIndex: familyIndex,
                warmState: slot === 3 ? "active" : "warm",
                warm: true,
                editorOpen: false,
                latencySamples: slot === 3 ? 240 : 0,
                latencyMs: slot === 3 ? 5 : 0,
            };
        }),
    }));

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
    families: FIXTURE_PATCHES,
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
/** MIDI fixture mirroring the ALSA devices on the rig. */
const MIDI: ControlMidiState = {
    inputs: [
        {
            id: "ALSA:4:1",
            name: "Launchpad Mini MK3 LPMiniMK3 MI",
            role: "launchpad",
            port: "MI",
            enabled: true,
            open: true,
            events: 0,
            lastEvent: "",
        },
        {
            id: "ALSA:4:0",
            name: "Launchpad Mini MK3 LPMiniMK3 DA",
            role: "launchpad",
            port: "DA",
            enabled: false,
            open: false,
            events: 0,
            lastEvent: "",
        },
        {
            id: "ALSA:5:0",
            name: "Keystation Mini 32 MK3 USB Audio",
            role: "keystation",
            port: "USB",
            enabled: false,
            open: false,
            events: 0,
            lastEvent: "",
        },
        {
            id: "ALSA:2:0",
            name: "Steinberg UR22mkII MIDI 1",
            role: "input",
            port: "",
            enabled: false,
            open: false,
            events: 0,
            lastEvent: "",
        },
    ],
    inputCount: 4,
    launchpad: {
        name: "Launchpad Mini MK3 LPMiniMK3 MI",
        port: "MI",
        connected: true,
        handshake: true,
        inputName: "Launchpad Mini MK3 LPMiniMK3 MI",
        outputName: "Launchpad Mini MK3 LPMiniMK3 MI",
        error: "",
    },
    keystation: {
        name: "Keystation Mini 32 MK3 USB Audio",
        port: "USB",
        connected: false,
        handshake: false,
        inputName: "",
        outputName: "",
        error: "",
    },
    padTest: { active: false, lit: 0, durationMs: 900 },
    error: "",
};

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
            { id: "piano", name: "Grand Piano", slot: 0, colorIndex: 0, warmState: "warm", latencySamples: 0, latencyMs: 0 },
            { id: "strings", name: "Warm Strings", slot: 1, colorIndex: 0, warmState: "warm", latencySamples: 72, latencyMs: 1.5 },
        ],
    },
    {
        id: "drums",
        name: "Drums",
        patches: [{ id: "kit", name: "Acoustic Kit", slot: 2, colorIndex: 1, warmState: "active", latencySamples: 0, latencyMs: 0 }],
    },
    {
        id: "bass",
        name: "Bass",
        patches: [{ id: "sub", name: "Sub Bass", slot: 3, colorIndex: 2, warmState: "warm", latencySamples: 240, latencyMs: 5 }],
    },
    {
        id: "pads",
        name: "Pads",
        patches: [{ id: "air", name: "Air Pad", slot: 4, colorIndex: 3, warmState: "warm", latencySamples: 120, latencyMs: 2.5 }],
    },
    {
        id: "lead",
        name: "Lead",
        patches: [{ id: "saw", name: "Saw Lead", slot: 5, colorIndex: 4, warmState: "warm", latencySamples: 48, latencyMs: 1 }],
    },
    {
        id: "fx",
        name: "FX",
        patches: [{ id: "riser", name: "Riser", slot: 6, colorIndex: 5, warmState: "warm", latencySamples: 96, latencyMs: 2 }],
    },
    {
        id: "vocal",
        name: "Vocal",
        patches: [{ id: "chop", name: "Vocal Chop", slot: 7, colorIndex: 6, warmState: "warm", latencySamples: 24, latencyMs: 0.5 }],
    },
    {
        id: "perc",
        name: "Percussion",
        patches: [{ id: "shaker", name: "Shaker", slot: null, colorIndex: 7, warmState: "cold" }],
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

/**
    Insert-chain fixture for the instrument page.

    Slot 3 is armed in `/state`, so its chain is the one the page renders: an
    EQ with no latency and a compressor with 156 samples, on top of 84 samples
    of instrument latency, for the 240 samples / 5 ms the patch already
    publishes.
*/
const CHAIN_INSTRUMENT_LATENCY = 84;
const COMPRESSOR_LATENCY = 156;

function fixtureInsert (index: number,
                        type: ControlFxInsert["type"],
                        id: string,
                        name: string,
                        latencySamples: number,
                        parameters: Record<string, number> = {}): ControlFxInsert
{
    return {
        index,
        type,
        kind: type === "vst3" ? "vst3" : "builtin",
        id,
        name,
        bypassed: false,
        wet: 1,
        dry: 0,
        latencySamples,
        parameters,
    };
}

/** Recompute indices and the latency budget of a fixture chain. */
function recalculateChain (chain: ControlFxChain): void
{
    chain.inserts = chain.inserts.map ((insert, index) => ({ ...insert, index }));
    chain.fxLatencySamples = chain.inserts.reduce (
        (total, insert) => total + (insert.bypassed || insert.wet === 0 ? 0 : insert.latencySamples),
        0);
    chain.latencySamples = chain.instrumentLatencySamples + chain.fxLatencySamples;
    chain.latencyMs = (chain.latencySamples * 1000) / 48000;
}

/** The installed-plug-in inventory fixture for `GET /plugins`. */
const PLUGIN_FIXTURE: ControlPluginList = {
    plugins: [
        {
            id: "com.instrumenthub.TestPlugin",
            name: "TestPlugin",
            description: "Instrument Hub test instrument",
            vendor: "Instrument Hub",
            version: "1.0.0",
            category: "Instrument",
            isInstrument: true,
            isEffect: false,
            format: "VST3",
            path: "/usr/lib/vst3/TestPlugin.vst3",
        },
        {
            id: "com.instrumenthub.TestReverb",
            name: "TestReverb",
            description: "Instrument Hub test effect",
            vendor: "Instrument Hub",
            version: "1.0.0",
            category: "Fx",
            isInstrument: false,
            isEffect: true,
            format: "VST3",
            path: "/usr/lib/vst3/TestReverb.vst3",
        },
        {
            id: "com.arturia.AnalogLab",
            name: "Analog Lab V",
            description: "Analog Lab",
            vendor: "Arturia",
            version: "5.10.0",
            category: "Instrument",
            isInstrument: true,
            isEffect: false,
            format: "VST3",
            path: "/usr/lib/vst3/Analog Lab V.vst3",
        },
    ],
    count: 3,
    total: 3,
    search: "",
    scanning: false,
    scanned: true,
    lastScanMs: 1_700_000_000_000,
    error: "",
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

function renderApp (store: AppStore, initialPath?: string): string
{
    return renderToStaticMarkup (
        <AppStoreProvider store={store}>
            <App {...(initialPath === undefined ? {} : { initialPath })} />
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

        const before = renderApp (store, "/audio");

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

        const after = renderApp (store, "/audio");

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

        const off = renderApp (store, "/audio");

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

    // The page loads its chain and the plug-in inventory from effects, which
    // do not run under `react-dom/server`; load them here so the artifact shows
    // the same data a browser would.
    const armedSlot = store.getSnapshot ().state?.armedSlot ?? -1;
    const instrumentSlot = instrumentSlotForRoute ("instrument", 3, armedSlot);

    if (instrumentSlot !== null)
        await store.loadFxChain (instrumentSlot);

    await store.loadPlugins ();

    const inventory = store.getSnapshot ().plugins;

    const html = renderApp (store);
    const audioHtml = renderApp (store, "/audio");
    const instrumentHtml = renderApp (store, "/instrument?slot=3");
    const pluginsHtml = renderApp (store, "/plugins");

    const suffix = fromFixture ? "snapshot" : "live";

    writeArtifact (`ui-state-${suffix}.html`, html);
    writeArtifact (`ui-audio-${suffix}.html`, audioHtml);
    writeArtifact (`ui-instrument-${suffix}.html`, instrumentHtml);
    writeArtifact (`ui-plugins-${suffix}.html`, pluginsHtml);

    check (html.length > 2000, "snapshot is non-blank");
    check (countOf (html, 'data-testid="family"') === 8, "snapshot shows 8 families");
    check (countOf (html, 'data-testid="grid-column"') === 8, "snapshot shows 8 patch columns");
    check (countOf (html, 'data-testid="grid-cell"') === 64, "snapshot shows a full 8×8 grid");
    check (countOf (html, 'data-fill="') === 64, "every one of the 64 cells carries a colour");
    check (countOf (html, 'data-testid="slot-meter"') === 64, "every patch has a level meter");
    check (countOf (html, 'data-testid="slot-latency"') === 64,
           "every patch shows its chain latency");
    check (html.includes ('data-testid="slot-latency" data-slot="3" data-latency-samples="240" data-latency-ms="5.000"'),
           "the API chain latency reaches the grid");
    check (countOf (html, 'data-armed="true"') === 1, "exactly one cell is armed");
    check (html.includes ('data-slot="3" data-armed="true"'), "the armed cell is slot 3");
    check (countOf (html, 'data-fill="#ff1f2f"') === 1, "the armed cell is painted bright red");
    check (html.includes ('data-phase="ready"'), "connection is ready");
    check (html.includes ("Connected"), "snapshot shows the connected status");
    check (html.includes ("Instrument Hub"), "snapshot shows the application name");
    check (html.includes ("0.1.0"), "snapshot shows the application version");
    check (html.includes ('data-testid="armed-slot"'), "snapshot shows the armed-slot widget");
    check (html.includes ('data-testid="connection-status"'), "snapshot shows the connection widget");
    check (html.includes ('data-testid="transport-bpm"'), "snapshot shows the transport widget");
    check (html.includes ('data-testid="instrument-grid"'), "snapshot shows the instrument grid");
    check (html.includes ('data-testid="app-nav"'), "snapshot shows the page navigation");
    check (html.includes ('data-testid="nav-grid" data-active="true"'),
           "the Grid tab is active on /grid");

    check (audioHtml.includes ('data-testid="audio-page"'), "the /audio route resolves to its page");
    check (audioHtml.includes ('data-testid="audio-settings"'), "audio page shows the audio settings panel");
    check (audioHtml.includes ('data-testid="device-type-select"'), "panel shows the device type control");
    check (audioHtml.includes ('data-testid="device-select"'), "panel shows the device control");
    check (audioHtml.includes ('data-testid="sample-rate-select"'), "panel shows the sample rate control");
    check (audioHtml.includes ('data-testid="buffer-size-select"'), "panel shows the buffer size control");
    check (audioHtml.includes ('data-testid="latency-buffer"'), "panel shows the buffer latency");
    check (audioHtml.includes ('data-testid="latency-device"'), "panel shows the device latency");
    check (audioHtml.includes ('data-testid="latency-chain"'), "panel shows the chain latency");
    check (audioHtml.includes ('data-testid="latency-total"'), "panel shows the total latency");
    check (audioHtml.includes ('data-testid="xrun-count"'), "panel shows the xrun counter");
    check (audioHtml.includes ('data-testid="measure-button"'), "panel shows the Measure button");
    check (audioHtml.includes ('data-open="true"'), "panel reports the device is open");
    check (audioHtml.includes ('data-testid="midi-panel"'), "the audio page shows the MIDI panel");
    check (audioHtml.includes ('data-testid="pad-test"'), "the MIDI panel has a pad test");
    check (audioHtml.includes ('data-testid="launchpad-status"'),
           "the MIDI panel shows the Launchpad status");
    check (audioHtml.includes ('data-testid="keystation-status"'),
           "the MIDI panel shows the Keystation status");

    if (fromFixture)
    {
        check (countOf (audioHtml, 'data-testid="midi-input"') === 4,
               "the MIDI panel lists every ALSA input");
        check (audioHtml.includes ('data-testid="midi-input" data-id="ALSA:4:1"'),
               "the Launchpad MI input is listed");
        check (audioHtml.includes ('data-testid="midi-input" data-id="ALSA:5:0"'),
               "the Keystation input is listed");
        check (audioHtml.includes ('data-testid="launchpad-status" data-connected="true" data-port="MI"'),
               "the Launchpad status names the MI port");
        check (audioHtml.includes ('data-testid="launchpad-port" data-port="MI">MI<'),
               "the Launchpad port pill reads MI");
        check (audioHtml.includes ('data-testid="launchpad-handshake" data-handshake="true"'),
               "the Launchpad programming handshake is shown");
        check (audioHtml.includes ('data-testid="keystation-status" data-connected="false" data-port="USB"'),
               "the Keystation is detected on its USB port");
        check (audioHtml.includes ('data-testid="pad-test-result" data-active="false" data-lit="0"'),
               "the pad test has not run yet");
    }
    check (audioHtml.includes ('data-testid="pairing"'), "desktop shows the tablet pairing card");
    check (audioHtml.includes ('data-testid="pairing" data-lan="false"'),
           "LAN mode is off by default");
    check (audioHtml.includes ('data-testid="lan-toggle" aria-pressed="false"'),
           "the LAN toggle starts off");
    check (! audioHtml.includes ('data-testid="pairing-qr"'),
           "no QR code is rendered while LAN mode is off");
    check (audioHtml.includes ('data-testid="pairing-token-state" data-paired="false"'),
           "an unpaired desktop reports no token");
    check (countOf (audioHtml, 'data-testid="output-channel"')
               === (store.getSnapshot ().audio?.outputChannels.length ?? 0),
           "panel maps every output channel the engine reports");

    check (instrumentHtml.includes ('data-testid="instrument-page" data-slot="3"'),
           "the /instrument route resolves to the armed patch");
    check (instrumentHtml.includes ('data-testid="instrument-name">Drawbar Organ<'),
           "the instrument page shows the patch at the queried slot");
    check (pluginsHtml.includes ('data-testid="plugins-page"'), "the /plugins route resolves");
    check (pluginsHtml.includes ('data-testid="plugin-search"'),
           "the plugins page has a search control");
    check (pluginsHtml.includes ('data-testid="plugin-rescan"'),
           "the plugins page has a rescan control");
    check (countOf (pluginsHtml, 'data-testid="plugin-row"') === (inventory?.plugins.length ?? 0),
           "the plugins page lists every scanned plug-in");
    check (pluginsHtml.includes ('data-testid="plugin-count"'),
           "the plugins page reports how many plug-ins it lists");
    check (! pluginsHtml.includes ('data-testid="plugin-assign"'),
           "the plugins page exposes no assignment control");

    // The markup must agree with the state the API actually returned.
    check (countOf (html, 'data-armed="true"') === (armedSlot >= 0 ? 1 : 0),
           `armed-slot highlight matches the fetched state (armedSlot ${String (armedSlot)})`);
    check ((armedSlot < 0) === html.includes ("armed none"),
           "footer reports the fetched arm state");

    if (fromFixture)
    {
        check (html.includes ("Drawbar Organ"), "snapshot shows the armed patch name");
        check (html.includes ("Studio Grand"), "snapshot shows the first slot name");
        check (html.includes ("128 BPM"), "snapshot shows the fetched tempo");
        check (html.includes ("Playing"), "snapshot shows the fetched transport state");
        check (pluginsHtml.includes ('data-testid="plugin-name">TestPlugin<'),
               "the plugins page shows the scanned TestPlugin");
        check (pluginsHtml.includes ('data-testid="plugin-vendor">Instrument Hub<'),
               "a plug-in row shows its vendor");
        check (pluginsHtml.includes ('data-testid="plugin-format">VST3<'),
               "a plug-in row shows its format");
        check (pluginsHtml.includes ('data-testid="plugin-path"')
                   && pluginsHtml.includes ("/usr/lib/vst3/TestPlugin.vst3"),
               "a plug-in row shows its bundle path");
        check (pluginsHtml.includes ('data-testid="plugin-used-by"'),
               "a plug-in assigned to a patch shows the used-by hint");
        check (pluginsHtml.includes ('data-patch="Celesta"'),
               "the used-by hint names the patch that uses the plug-in");
        check (pluginsHtml.includes ('data-testid="plugin-unused"'),
               "an unassigned plug-in says it is not used by a patch");
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
    check (countOf (html, 'data-fill="') === 64, "every grid cell carries a colour");
    check (countOf (html, 'data-fill="#ff1f2f"') === 1, "the armed cell is bright red");
    check (html.includes ('data-slot="4" data-armed="false" data-empty="false"')
               && html.includes ('data-colour-index="3"'),
           "an unarmed cell keeps its patch colour");
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
    check (/\.grid__cell--armed\s*\{[^}]*background:\s*#ff1f2f/s.test (css),
           "the armed cell's CSS background is bright red");
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

function routeTest (): void
{
    check (SHELL_ROUTES.includes ("grid") && SHELL_ROUTES.includes ("instrument")
               && SHELL_ROUTES.includes ("plugins") && SHELL_ROUTES.includes ("audio")
               && SHELL_ROUTES.includes ("remote"),
           "the shell declares every page route");

    for (const route of SHELL_ROUTES)
        check (routeFromPath (pathForRoute (route)) === route,
               `/${route} resolves to itself`);

    check (routeFromPath ("/") === "grid" && routeFromPath ("/nope") === "grid",
           "the root and unknown paths land on the grid");
    check (routeFromPath ("/audio/") === "audio", "a trailing slash still resolves");
    check (instrumentPathForSlot (3) === "/instrument?slot=3",
           "an instrument selection is encoded in the URL");
    check (instrumentPathForSlot (null) === "/instrument",
           "no selection still resolves the instrument page");
    check (slotFromSearch ("?slot=12") === 12, "the instrument page reads its slot");
    check (slotFromSearch ("?code=123456") === null, "an unrelated query selects no patch");
    check (slotFromSearch ("?slot=-1") === null, "a negative slot is not a selection");

    const actions: string[] = [];

    armAndOpenInstrument (
        5,
        slot => { actions.push (`arm:${String (slot)}`); },
        path => { actions.push (`nav:${path}`); });

    check (actions.join ("|") === "arm:5|nav:/instrument?slot=5",
           "one click arms the patch and routes to its instrument page");
}

async function clickArmsAndRoutesTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        await store.refresh ();

        let navigated = "";
        let armResult: Promise<boolean> = Promise.resolve (false);

        armAndOpenInstrument (
            5,
            slot => { armResult = store.armSlot (slot); },
            path => { navigated = path; });

        check (navigated === "/instrument?slot=5",
               "clicking a cell routes to /instrument?slot=5");
        check (store.getSnapshot ().state?.armedSlot === 5,
               "clicking a cell arms the patch immediately");

        const optimistic = renderApp (store);
        check (optimistic.includes ('data-slot="5" data-armed="true"'),
               "the clicked cell is red on the next render");

        check (await armResult, "POST /arm confirms the clicked patch");
        check (server.armRequests.length === 1 && server.armRequests[0]?.slot === 5,
               "the engine received the clicked slot");
    }
    finally
    {
        await server.close ();
    }
}

async function colourEditTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        await store.refresh ();

        const initial = renderApp (store);
        check (/data-slot="3"[^>]*data-colour-index="0"/.test (initial),
               "slot 3 starts on palette colour 0");

        const pending = store.updatePatch (3, { colorIndex: 5 });
        const optimistic = renderApp (store);
        check (/data-slot="3"[^>]*data-colour-index="5"/.test (optimistic),
               "the cell recolours before PATCH answers");

        check (await pending, "PATCH /patches/3 accepts the colour");
        check (server.patchRequests.length === 1
                   && server.patchRequests[0]?.slot === 3
                   && server.patchRequests[0]?.patch.colorIndex === 5,
               "the engine received colorIndex 5 for slot 3");

        const recoloured = renderApp (store);
        check (recoloured.includes (`data-slot="3" data-armed="true" data-empty="false"`)
                   && recoloured.includes (`data-colour-index="5" data-colour="${PATCH_COLOURS[5]}"`),
               "the recoloured armed cell shows the new palette colour");

        const familyChanged = await store.updateFamilyColour ("drums", 12);
        const drums = store.getSnapshot ().state?.families.find (family => family.id === "drums");

        check (familyChanged && drums?.patches.every (patch => patch.colorIndex === 12) === true,
               "a family colour change reaches every patch in the row");
        check (server.patchRequests.slice (1).map (request => request.slot).join (",")
                   === "8,9,10,11,12,13,14,15",
               "the family change PATCHes all eight patches");

        const familyHtml = renderApp (store);
        check (/data-family="drums"[^>]*data-colour-index="12"/.test (familyHtml),
               "the family header reflects the new colour");
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
        updatePatch: async (): Promise<ControlState> =>
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
        changePatchSource: async () =>
        {
            throw new Error ("connection refused");
        },
        openPatchEditor: async () =>
        {
            throw new Error ("connection refused");
        },
        closePatchEditor: async () =>
        {
            throw new Error ("connection refused");
        },
        capturePatchState: async () =>
        {
            throw new Error ("connection refused");
        },
        getFxChain: async () =>
        {
            throw new Error ("connection refused");
        },
        addFxInsert: async () =>
        {
            throw new Error ("connection refused");
        },
        replaceFxChain: async () =>
        {
            throw new Error ("connection refused");
        },
        updateFxInsert: async () =>
        {
            throw new Error ("connection refused");
        },
        removeFxInsert: async () =>
        {
            throw new Error ("connection refused");
        },
        reorderFxChain: async () =>
        {
            throw new Error ("connection refused");
        },
        listPlugins: async () =>
        {
            throw new Error ("connection refused");
        },
        rescanPlugins: async () =>
        {
            throw new Error ("connection refused");
        },
        getMidi: async () =>
        {
            throw new Error ("connection refused");
        },
        setMidiInput: async () =>
        {
            throw new Error ("connection refused");
        },
        runMidiPadTest: async () =>
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

        const initial = renderApp (store, "/audio");

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

        const after = renderApp (store, "/audio");

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

        const measuredHtml = renderApp (store, "/audio");

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

function midiModelTest (): void
{
    check (midiRoleLabel ("launchpad") === "Launchpad", "the Launchpad role has a label");
    check (midiRoleLabel ("keystation") === "Keyboard", "the Keystation is shown as a keyboard");
    check (midiRoleLabel ("input") === "MIDI input", "any other port is a MIDI input");

    check (midiInputSummary (MIDI.inputs) === "4 inputs · 1 enabled",
           "the summary counts inputs and enabled switches");
    check (midiInputSummary ([MIDI.inputs[0]!]) === "1 input · 1 enabled",
           "the singular summary is used for one input");
    check (midiInputSummary ([]) === "No MIDI inputs", "an empty device list is explained");

    check (midiInputState (MIDI.inputs[0]!) === "Open", "an enabled open input reads Open");
    check (midiInputState (MIDI.inputs[2]!) === "Off", "a disabled input reads Off");

    check (midiPortLabel ("MI") === "MI" && midiPortLabel ("  ") === "—",
           "the port label falls back to a dash");
    check (midiPortState (MIDI.launchpad, true) === "Connected",
           "a connected port says Connected");
    check (midiPortState (MIDI.keystation, true) === "Detected · not connected",
           "a detected but closed port says so");

    check (isLaunchpadDaInput (MIDI.inputs[1]!) && ! isLaunchpadDaInput (MIDI.inputs[0]!),
           "the inert DA port is distinguished from MI");

    check (midiInputForRole (MIDI.inputs, "keystation")?.id === "ALSA:5:0",
           "the Keystation row is found by role");
    check (midiInputForRole (MIDI.inputs, "launchpad")?.id === "ALSA:4:1",
           "the Launchpad row prefers the MI port");
    check (midiInputForRole (MIDI.inputs, "input")?.name === "Steinberg UR22mkII MIDI 1",
           "a plain input is found by role");

    check (padTestLabel (null, null) === "Not run yet", "a fresh page has not run the test");
    check (padTestLabel ({ active: true, lit: 64, durationMs: 900 }, null) === "64 pads lit",
           "a running test reports the lit pad count");
    check (padTestLabel ({ active: false, lit: 0, durationMs: 900 }, "no Launchpad") === "no Launchpad",
           "a rejected test shows the engine reason");

    check (canRunPadTest (MIDI), "the pad test is available with a connected Launchpad");
    check (! canRunPadTest ({ ...MIDI, launchpad: { ...MIDI.launchpad, connected: false } }),
           "the pad test is unavailable without a Launchpad");
}

async function midiPageTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        check (await store.refresh (), "the MIDI fixture loads GET /midi");
        check (store.getSnapshot ().midi?.inputs.length === 4,
               "the store holds all four ALSA inputs");
        check (store.getSnapshot ().midi?.launchpad.port === "MI",
               "the Launchpad is reported on the MI port");

        const initial = renderApp (store, "/audio");

        writeArtifact ("ui-audio-midi-snapshot.html", initial);

        check (initial.includes ('data-testid="midi-panel"'), "the /audio page renders the MIDI panel");
        check (countOf (initial, 'data-testid="midi-input"') === 4,
               "the panel lists every ALSA input");
        check (initial.includes ('data-testid="midi-input" data-id="ALSA:4:1"'),
               "the Launchpad MI input is listed");
        check (initial.includes ('data-testid="midi-input" data-id="ALSA:4:1"')
                   && initial.includes ('data-role="launchpad"'),
               "the Launchpad input carries its role");
        check (initial.includes ('data-testid="midi-input" data-id="ALSA:5:0"'),
               "the Keystation input is listed");
        check (initial.includes ('data-id="ALSA:4:0"') && initial.includes ("MI only"),
               "the inert DA port is shown as MI-only");
        check (/data-testid="midi-input-toggle" data-id="ALSA:4:0"[^>]*disabled/.test (initial),
               "the DA checkbox cannot be switched on");
        check (initial.includes ('data-testid="launchpad-status" data-connected="true" data-port="MI"'),
               "the Launchpad status names the MI port");
        check (initial.includes ('data-testid="launchpad-port" data-port="MI">MI<'),
               "the Launchpad port pill reads MI");
        check (initial.includes ("LPMiniMK3 MI"),
               "the Launchpad device name is visible");
        check (initial.includes ('data-testid="launchpad-handshake" data-handshake="true"'),
               "the programmer-mode echo is shown");
        check (initial.includes ('data-testid="keystation-status" data-connected="false" data-port="USB"'),
               "the Keystation is detected but not connected");
        check (initial.includes ('data-testid="keystation-port" data-port="USB">USB<'),
               "the Keystation port pill reads USB");
        check (initial.includes ('data-testid="pad-test"'), "the page offers the pad test");
        check (initial.includes ('data-testid="pad-test-result" data-active="false" data-lit="0"'),
               "the pad test has not run yet");
        check (/data-testid="pad-test"[^>]*disabled/.test (initial) === false,
               "the pad test is available while the Launchpad is connected");

        // --- Enable a MIDI input -------------------------------------------
        check (await store.toggleMidiInput ("ALSA:5:0", true),
               "enabling the Keystation succeeds");
        check (server.midiRequests.length === 1
                   && server.midiRequests[0]?.id === "ALSA:5:0"
                   && server.midiRequests[0]?.enabled === true,
               "the UI sent { id, enabled: true } to POST /midi/inputs");
        check (store.getSnapshot ().midi?.keystation.connected === true,
               "the store adopts the opened Keystation");
        check (store.getSnapshot ().midi?.inputs.find (input => input.id === "ALSA:5:0")?.enabled === true,
               "the Keystation row is switched on");

        const enabled = renderApp (store, "/audio");

        check (/data-testid="midi-input" data-id="ALSA:5:0"[^>]*data-enabled="true"/.test (enabled),
               "the Keystation row renders as enabled");
        check (/data-testid="midi-input-toggle" data-id="ALSA:5:0"[^>]*checked/.test (enabled),
               "the Keystation checkbox is checked");
        check (enabled.includes ("2 enabled"), "the summary follows the second switch");

        // --- Pad test -------------------------------------------------------
        check (await store.runPadTest (), "POST /midi/pad-test starts the grid test");
        check (server.padTestRequests === 1, "the pad test reached the engine");
        check (store.getSnapshot ().midi?.padTest.active === true,
               "the store adopts the running test");
        check (store.getSnapshot ().midi?.padTest.lit === 64,
               "the test reports all 64 pads lit");

        const testing = renderApp (store, "/audio");

        check (testing.includes ('data-testid="midi-panel"')
                   && testing.includes ('data-pad-test-active="true"'),
               "the panel reports the running test");
        check (testing.includes ('data-testid="pad-test-result" data-active="true" data-lit="64"'),
               "the result names the 64 lit pads");
        check (testing.includes ("64 pads lit"), "the result is readable");
        check (testing.includes ("Lighting…"), "the button shows the test is running");

        // --- Failure surface -------------------------------------------------
        const failing = new AppStore (
            createControlApi ({ baseUrl: "http://127.0.0.1:1" }),
            { pollIntervalMs: 60_000 });

        check (! await failing.loadMidi (), "a failed MIDI read is reported");
        check (failing.getSnapshot ().midiError !== null,
               "the store keeps the MIDI failure message");

        const failureHtml = renderApp (failing, "/audio");

        check (failureHtml.includes ('data-testid="midi-panel" data-loading="true"'),
               "the MIDI panel shows its loading/failure state");
        check (failureHtml.includes ('data-testid="midi-loading"'),
               "the failure is shown in place of the device list");

        const loadingHtml = renderToStaticMarkup (
            <MidiPanel midi={null} loading />);

        check (loadingHtml.includes ('data-testid="midi-loading"'),
               "a first load shows the reading state");
        check (loadingHtml.includes ('aria-busy="true"'), "the loading panel is marked busy");
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
    check (html.includes ('data-theme="stage"'), "remote uses the stage theme");
    check (html.includes ('data-contrast="high"'), "the stage remote starts in high contrast");
    check (html.includes ('data-testid="remote-contrast"'),
           "the remote exposes a contrast control");
    check (! html.includes ('data-testid="remote-banner"'),
           "a healthy remote shows no reconnect banner");
    check (! html.includes ('data-testid="remote-wake-lock"'),
           "the server-rendered remote claims no wake lock");

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

function remoteBannerTest (): void
{
    const live: RemoteLinkSnapshot = {
        connected: true,
        everConnected: true,
        lastLiveAt: 1,
        failures: 0,
    };

    const lost: RemoteLinkSnapshot = { ...live, connected: false, failures: 1 };

    check (! remoteBanner ("ready", live).visible, "a live WS shows no banner");
    check (! remoteBanner ("connecting", live).visible, "connecting does not show the banner");

    const lostBanner = remoteBanner ("ready", lost);

    check (lostBanner.visible && lostBanner.tone === "warning",
           "a lost WS shows the reconnecting banner");
    check (lostBanner.title === "Live link lost", "the banner names the lost link");
    check (lostBanner.detail.includes ("Reconnecting automatically"),
           "the banner promises an automatic reconnect");
    check (! remoteBanner ("ready", { ...INITIAL_LINK }).visible,
           "no banner before the first connection");

    const offline = remoteBanner ("offline", live);

    check (offline.visible && offline.tone === "critical",
           "an offline control API shows the critical banner");
    check (offline.title === "Engine unreachable", "the critical banner names the engine");
}

class FakeWakeLockSentinel implements WakeLockSentinelLike
{
    released = false;
    private readonly listeners: (() => void)[] = [];

    async release (): Promise<void>
    {
        this.released = true;

        for (const listener of this.listeners)
            listener ();
    }

    addEventListener (_type: "release", listener: () => void): void
    {
        this.listeners.push (listener);
    }
}

class FakeWakeLockNavigator implements WakeLockCapableNavigator
{
    readonly requests: string[] = [];
    readonly sentinels: FakeWakeLockSentinel[] = [];
    fail = false;

    readonly wakeLock = {
        request: async (type: "screen"): Promise<WakeLockSentinelLike> =>
        {
            this.requests.push (type);

            if (this.fail)
                throw new Error ("wake lock denied");

            const sentinel = new FakeWakeLockSentinel ();
            this.sentinels.push (sentinel);

            return sentinel;
        },
    };
}

async function remoteWakeLockTest (): Promise<void>
{
    const none = new WakeLockController ({ navigator: {} });

    check (await none.start () === "unavailable",
           "a browser without the wake-lock API reports unavailable");

    const fake = new FakeWakeLockNavigator ();
    const statuses: WakeLockStatus[] = [];
    const controller = new WakeLockController ({
        navigator: fake,
        onStatus: status => { statuses.push (status); },
    });

    check (await controller.start () === "active", "the remote asks for a screen wake lock");
    check (fake.requests.length === 1 && fake.requests[0] === "screen",
           "the request is for the screen");
    check (controller.status === "active", "an acquired wake lock reports active");
    check (statuses.includes ("requesting"), "the status passes through requesting");

    check (await controller.visibilityChanged (false) === "idle",
           "hiding the page drops the lock");
    check (fake.sentinels[0]?.released === true, "the hidden page's sentinel is released");

    check (await controller.visibilityChanged (true) === "active",
           "returning to the page re-acquires the lock");
    check (fake.requests.length === 2, "a second request is made after visibility returns");

    await controller.release ();

    check (controller.status === "idle", "releasing the controller returns to idle");
    check (fake.sentinels.every (sentinel => sentinel.released),
           "every sentinel is released");

    const blocked = new FakeWakeLockNavigator ();
    blocked.fail = true;

    const denied = new WakeLockController ({ navigator: blocked });

    check (await denied.start () === "denied", "a rejected request reports denied");
    check (denied.status === "denied", "the denied status is kept for the badge");

    await denied.release ();
}

function remoteContrastTest (): void
{
    const storage = new MemoryStorage ();

    check (DEFAULT_REMOTE_CONTRAST === "high", "the stage remote defaults to high contrast");
    check (loadRemoteContrast (null) === "high", "contrast loads high without storage");
    check (loadRemoteContrast (storage) === "high", "a fresh browser gets the high-contrast theme");

    saveRemoteContrast (storage, "standard");
    check (loadRemoteContrast (storage) === "standard", "the contrast choice is persisted");

    check (nextContrast ("high") === "standard" && nextContrast ("standard") === "high",
           "the contrast toggle flips both ways");
    check (loadRemoteContrast ({ getItem: () => "neon", setItem: () => {} }) === "high",
           "an unknown stored value falls back to high contrast");
    check (loadRemoteContrast ({
               getItem: () => { throw new Error ("storage blocked"); },
               setItem: () => {},
           }) === "high",
           "blocked storage falls back to high contrast");

    const high = renderToStaticMarkup (
        <RemoteView
            model={remoteFromState (FIXTURE)}
            connection="ready"
            contrast="high"
            wakeLock="active"
        />);

    check (high.includes ('data-theme="stage"'), "remote declares the stage theme");
    check (high.includes ('data-contrast="high"'), "remote renders the high-contrast mode");
    check (high.includes ('data-testid="remote-contrast" data-contrast="high" aria-pressed="true"'),
           "the contrast control reports high contrast");
    check (high.includes (">High contrast<"), "the contrast control is labelled");
    check (high.includes ('data-testid="remote-wake-lock" data-state="active"'),
           "the remote reports the screen wake lock");
    check (high.includes (">Screen on<"), "the wake-lock badge explains itself");

    const standard = renderToStaticMarkup (
        <RemoteView
            model={remoteFromState (FIXTURE)}
            connection="ready"
            contrast="standard"
        />);

    check (standard.includes ('data-contrast="standard"')
               && standard.includes ('aria-pressed="false"'),
           "standard contrast renders when selected");
    check (standard.includes (">Standard<"), "the standard mode is labelled");
    check (! standard.includes ('data-testid="remote-wake-lock"'),
           "no wake-lock badge while the lock is idle");

    const css = readFileSync (
        fileURLToPath (new URL ("../src/features/remote/remote.css", import.meta.url)), "utf8");

    check (/\.remote\[data-contrast="high"\]\s*\{[^}]*--remote-bg:\s*#000000/s.test (css),
           "the high-contrast palette paints a pure black background");
    check (/\.remote\[data-contrast="high"\]\s*\{[^}]*--remote-text:\s*#ffffff/s.test (css),
           "the high-contrast palette paints pure white text");
    check (/\.remote\[data-contrast="high"\]\s+button\s*\{[^}]*border-color:\s*#ffffff/s.test (css),
           "high-contrast buttons get a white outline");
    check (/\.remote__banner--warning/.test (css) && /\.remote__banner--critical/.test (css),
           "both reconnect banner tones have styles");
}

async function remoteLinkTest (): Promise<void>
{
    const sockets: FakeMeterSocket[] = [];
    const link = new MeterLink (new MeterStore ({
        url: "ws://127.0.0.1:9/meters",
        socketFactory: () =>
        {
            const socket = new FakeMeterSocket ();
            sockets.push (socket);
            return socket;
        },
        reconnectBaseMs: 250,
        reconnectMaxMs: 250,
    }));

    link.start ();

    check (sockets.length === 1, "the remote opens one live WS");

    sockets[0]!.emit ("open", {});
    check (link.getSnapshot ().connected, "the live WS reports connected");
    check (link.getSnapshot ().everConnected, "the remote remembers the link came up");

    const liveHtml = renderToStaticMarkup (
        <RemoteView
            model={remoteFromState (FIXTURE)}
            connection="ready"
            link={link.getSnapshot ()}
        />);

    check (! liveHtml.includes ('data-testid="remote-banner"'),
           "no banner while the WS is up");

    const killedAt = Date.now ();
    sockets[0]!.close ();

    check (! link.getSnapshot ().connected, "killing the WS marks the link down");
    check (link.getSnapshot ().failures === 1, "the drop is counted");

    const downHtml = renderToStaticMarkup (
        <RemoteView
            model={remoteFromState (FIXTURE)}
            connection="ready"
            link={link.getSnapshot ()}
        />);

    check (downHtml.includes ('data-testid="remote-banner"'),
           "killing the WS shows the reconnect banner");
    check (downHtml.includes ('data-tone="warning"'),
           "the banner warns instead of blocking the set list");
    check (downHtml.includes ("Live link lost"), "the banner names the lost link");

    await waitFor (() => sockets.length >= 2, 3000);

    sockets[1]!.emit ("open", {});
    const recoveredMs = Date.now () - killedAt;

    check (recoveredMs < 3000,
           `the WS reconnects within 3 s (${String (recoveredMs)} ms)`);
    check (link.getSnapshot ().connected, "the reconnected WS reports connected");

    const recoveredHtml = renderToStaticMarkup (
        <RemoteView
            model={remoteFromState (FIXTURE)}
            connection="ready"
            link={link.getSnapshot ()}
        />);

    check (! recoveredHtml.includes ('data-testid="remote-banner"'),
           "the banner clears after the automatic reconnect");

    link.stop ();

    check (! link.getSnapshot ().connected && ! link.getSnapshot ().everConnected,
           "stopping the link resets its state");
}

async function remoteOptimisticTest (): Promise<void>
{
    const server = await startFixtureServer ({ armDelayMs: 250 });

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        await store.refresh ();

        const before = renderToStaticMarkup (
            <RemoteView model={remoteFromState (store.getSnapshot ().state)} connection="ready" />);

        check (before.includes ('data-testid="remote-set-item" data-slot="3" data-armed="true"'),
               "the remote starts on the armed slot");

        const startedAt = Date.now ();
        const pendingArm = store.armSlot (5);

        const optimistic = renderToStaticMarkup (
            <RemoteView model={remoteFromState (store.getSnapshot ().state)} connection="ready" />);

        check (optimistic.includes ('data-testid="remote-set-item" data-slot="5" data-armed="true"'),
               "the remote highlight moves before POST /arm answers");
        check (optimistic.includes ('data-testid="remote-current-name">Analog Brass<'),
               "the now-playing card follows the tap optimistically");
        check (! optimistic.includes ('data-slot="3" data-armed="true"'),
               "the old highlight is gone immediately");
        check (Date.now () - startedAt < 200,
               "the highlight moves before the delayed arm response");

        check (await pendingArm, "POST /arm confirms the optimistic arm");
        check (server.armRequests.length === 1 && server.armRequests[0]?.slot === 5,
               "the engine received the tapped slot");

        const pendingTransport = store.updateTransport ({ playing: false });

        const paused = renderToStaticMarkup (
            <RemoteView model={remoteFromState (store.getSnapshot ().state)} connection="ready" />);

        check (paused.includes ('data-testid="remote-transport-state">Stopped<'),
               "Play/Pause flips before POST /transport answers");
        check (paused.includes ('data-testid="remote-play"') && paused.includes (">Play<"),
               "the play button flips optimistically");

        check (await pendingTransport, "POST /transport confirms the optimistic pause");
        check (server.transportRequests.at (-1)?.playing === false,
               "the engine received playing: false");
    }
    finally
    {
        await server.close ();
    }
}

function instrumentModelTest (): void
{
    check (gainFromDb (0) === 1, "0 dB is unity gain");
    check (Math.abs (gainFromDb (-20) - 0.1) < 0.0001, "-20 dB is 0.1 linear gain");
    check (gainFromDb (FADER_MIN_DB) === 0, "the fader floor is silence");
    check (formatDb (0) === "0.0 dB" && formatDb (-18) === "-18.0 dB", "dB readout is fixed width");
    check (formatPan (-0.4) === "L 40" && formatPan (0) === "C" && formatPan (0.25) === "R 25",
           "balance reads as L/C/R with a percentage");
    check (clampPan (2) === 1 && clampPan (-2) === -1, "balance is clamped to ±1");
    check (wetPercent (0.355) === 36, "wet is a whole percentage");

    check (moveOrder (3, 2, 0).join (",") === "2,0,1", "moving the last insert to the front builds a permutation");
    check (moveOrder (3, 0, 2).join (",") === "1,2,0", "moving the first insert to the end builds a permutation");
    check (moveOrder (3, -5, 99).join (",") === "1,2,0", "out-of-range moves are clamped");
    check (moveOrder (0, 0, 0).length === 0, "an empty chain has no order");

    const chain: ControlFxChain = {
        slot: 3,
        instrumentLatencySamples: 84,
        fxLatencySamples: 156,
        latencySamples: 240,
        latencyMs: 5,
        inserts: [
            {
                index: 0, type: "eq", kind: "builtin", id: "eq", name: "EQ",
                bypassed: false, wet: 1, dry: 0, latencySamples: 0, parameters: {},
            },
            {
                index: 1, type: "compressor", kind: "builtin", id: "compressor",
                name: "Compressor", bypassed: false, wet: 1, dry: 0,
                latencySamples: 156, parameters: {},
            },
        ],
    };

    const bypassed = chainWithBypass (chain, 1, true);

    check (bypassed.fxLatencySamples === 0 && bypassed.latencySamples === 84,
           "bypassing an insert drops its latency from the budget");
    check (bypassed.latencyMs === 84 * (5 / 240), "the millisecond readout follows the sample total");

    const removed = chainWithoutInsert (chain, 0);

    check (removed.inserts.length === 1 && removed.inserts[0]?.index === 0,
           "removing the first insert renumbers the chain");

    const moved = chainWithMove (chain, 1, 0);

    check (moved.inserts[0]?.type === "compressor" && moved.inserts[1]?.index === 1,
           "moving an insert reorders the chain and renumbers it");

    const plugins: ControlPluginList = PLUGIN_FIXTURE;

    check (instrumentPlugins (plugins.plugins).length === 2
               && instrumentPlugins (plugins.plugins).every (plugin => plugin.isInstrument),
           "only instrument plug-ins are offered as a source");
    check (effectPlugins (plugins.plugins).length === 1
               && effectPlugins (plugins.plugins)[0]?.name === "TestReverb",
           "only effect plug-ins are offered as an insert");

    const source = sourceRequestForPlugin (plugins.plugins[0]!);

    check (source.kind === "plugin" && source.path === "/usr/lib/vst3/TestPlugin.vst3",
           "a plug-in source request carries the bundle path");
    check (source.pluginManufacturer === "Instrument Hub" && source.pluginVersion === "1.0.0",
           "the plug-in identity reaches the source request");
    check (sourceRequestForInitTone ().kind === "init-tone", "the built-in tone is a source request");
    check (insertRequestForBuiltin ("delay").type === "delay", "a built-in insert is addressable by type");
    check (insertRequestForPlugin (plugins.plugins[1]!).type === "vst3", "a plug-in insert uses the vst3 type");

    check (instrumentSlotForRoute ("instrument", null, 3) === 3,
           "the instrument page follows the armed slot");
    check (instrumentSlotForRoute ("instrument", 1, 3) === 3,
           "the controller selection beats a stale ?slot link");
    check (instrumentSlotForRoute ("instrument", 1, -1) === 1,
           "a link still selects a patch while nothing is armed");
    check (instrumentSlotForRoute ("grid", null, 3) === null,
           "other routes never resolve to an instrument slot");
    check (followsArmedSlot (3) && ! followsArmedSlot (-1),
           "the page follows the controller whenever a patch is armed");
    check (isPluginSource (null) === false, "no patch has no plug-in source");
    check (sourceKindLabel ("plugin") === "VST3 plug-in", "the source kind has a human label");
}

async function instrumentPageTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        check (await store.refresh (), "the instrument fixture loads GET /state");
        check (await store.loadFxChain (3), "GET /patches/3/fx returns the insert chain");
        check (await store.loadPlugins (), "GET /plugins returns the installed inventory");
        check (server.pluginRequests.length === 1, "the page fetched the inventory once");

        const initial = renderApp (store, "/instrument?slot=3");

        writeArtifact ("ui-instrument-snapshot.html", initial);

        check (initial.includes ('data-testid="instrument-page" data-slot="3"'),
               "the instrument page renders the queried slot");
        check (initial.includes ('data-testid="instrument-name">Drawbar Organ<'),
               "the page shows the patch's real name");
        check (initial.includes ("Keys · slot 04 · armed · following the controller"),
               "the page shows family, slot and arm state");
        check (initial.includes ('data-testid="instrument-warm-state" data-warm="true" data-warm-state="active"'),
               "the warm state comes from /state");
        check (initial.includes ('data-testid="instrument-latency" data-latency-samples="240" data-latency-ms="5.000"'),
               "the chain latency comes from /state");
        check (initial.includes ("5.00 ms · 240 smp"), "latency is readable in ms and samples");
        check (initial.includes ('data-testid="volume-fader" data-slot="3" data-db="0.00"'),
               "the fader starts at the patch's real volume");
        check (initial.includes ('data-testid="volume-value">0.0 dB<'), "the fader has a dB readout");
        check (initial.includes ('data-testid="volume-gain" data-gain="1.000000"'),
               "the page publishes the effective linear gain");
        check (initial.includes ('data-testid="pan-value">C<'), "the balance starts centred");
        check (initial.includes ('data-testid="mute-button" data-slot="3" data-muted="false"'),
               "mute reflects the patch");
        check (initial.includes ('data-testid="solo-button" data-slot="3" data-solo="false"'),
               "solo reflects the patch");
        check (initial.includes ('data-testid="instrument-colour" data-colour-index="0"'),
               "the patch colour reaches the page");

        check (initial.includes ('data-testid="instrument-source" data-kind="init-tone"'),
               "the page names the declared instrument source");
        check (initial.includes ('data-testid="source-open-editor"'),
               "the source has an editor control");
        check (initial.includes ('data-testid="source-save-state"'),
               "the page offers an explicit save-sound action");
        check (/data-testid="source-save-state"[^>]*disabled/.test (initial),
               "a generated source cannot save a plug-in sound");
        check (initial.includes ("Only plug-in sources have a sound state to save"),
               "the disabled save explains why");
        check (initial.includes ('data-testid="source-change"'), "the source can be changed");
        check (initial.includes ('value="/usr/lib/vst3/TestPlugin.vst3"'),
               "the source picker lists installed instrument plug-ins");
        check (! initial.includes ('value="/usr/lib/vst3/TestReverb.vst3"'),
               "the source picker excludes effect plug-ins");

        check (initial.includes ('data-testid="fx-chain" data-slot="3" data-count="2"'),
               "the insert chain renders both inserts");
        check (initial.includes ('data-latency-samples="240" data-latency-ms="5.000"'),
               "the chain publishes its latency budget");
        check (initial.includes ('data-testid="fx-latency-instrument">84 smp<'),
               "the chain separates instrument latency");
        check (initial.includes ('data-testid="fx-latency-inserts">156 smp<'),
               "the chain separates insert latency");
        check (initial.includes ('data-testid="fx-insert" data-index="0" data-type="eq"'),
               "the first insert is the EQ");
        check (initial.includes ('data-testid="fx-insert" data-index="1" data-type="compressor"'),
               "the second insert is the compressor");
        check (initial.includes ('data-testid="fx-bypass"')
                   && initial.includes ('title="Bypass EQ"')
                   && initial.includes ('aria-pressed="true"'),
               "an insert starts enabled with a bypass control");
        check (initial.includes ('value="builtin:eq"') && initial.includes ('value="plugin:com.instrumenthub.TestReverb"'),
               "the add control offers built-ins and installed effects");

        // --- The fader writes the real level to the engine -------------------
        check (await store.updatePatch (3, { volumeDb: -18 }), "moving the fader succeeds");
        check (server.patchRequests.length === 1
                   && server.patchRequests[0]?.slot === 3
                   && server.patchRequests[0]?.patch.volumeDb === -18,
               "the fader sends volumeDb: -18 to PATCH /patches/3");

        const quiet = renderApp (store, "/instrument?slot=3");

        check (quiet.includes ('data-testid="volume-fader" data-slot="3" data-db="-18.00"'),
               "the fader shows the new level");
        check (quiet.includes ('data-testid="volume-gain" data-gain="0.125893"'),
               "-18 dB resolves to 0.1259 linear gain");
        check (quiet.includes ('data-volume-db="-18.00" data-gain="0.125893"'),
               "the mixer card publishes the level the engine applies");

        check (await store.updatePatch (3, { mute: true }), "muting succeeds");
        check (server.patchRequests.at (-1)?.patch.mute === true, "mute reaches the engine");
        check (renderApp (store, "/instrument?slot=3").includes (
                   'data-testid="mute-button" data-slot="3" data-muted="true"'),
               "the mute button reflects the change");

        check (await store.updatePatch (3, { solo: true }), "soloing succeeds");
        check (renderApp (store, "/instrument?slot=3").includes (
                   'data-testid="solo-button" data-slot="3" data-solo="true"'),
               "the solo button reflects the change");

        check (await store.updatePatch (3, { colorIndex: 7 }), "recolouring succeeds");
        check (renderApp (store, "/instrument?slot=3").includes (
                   'data-testid="instrument-colour" data-colour-index="7"'),
               "the colour swatch follows the patch");

        // --- The chain editor -----------------------------------------------
        check (await store.addFxInsert (3, insertRequestForBuiltin ("compressor")),
               "adding a compressor succeeds");
        check (server.chain.inserts.length === 3 && server.chain.latencySamples === 396,
               "the engine added the insert and summed its latency");

        const added = renderApp (store, "/instrument?slot=3");

        check (added.includes ('data-testid="fx-chain" data-slot="3" data-count="3"'),
               "the new insert appears in the chain");
        check (added.includes ('data-latency-samples="396"'), "the chain latency grows with the insert");

        check (await store.moveFxInsert (3, 2, 0), "moving the last insert to the front succeeds");
        check (server.fxRequests.at (-1)?.method === "POST"
                   && (server.fxRequests.at (-1)?.body.order as number[]).join (",") === "2,0,1",
               "reordering sends a permutation to /fx/reorder");
        check (server.chain.inserts[0]?.index === 0 && server.chain.inserts[0]?.type === "compressor",
               "the engine applied the new order");

        const moved = renderApp (store, "/instrument?slot=3");

        check (/data-testid="fx-insert" data-index="0" data-type="compressor"/.test (moved),
               "the page renders the reordered chain");

        check (await store.updateFxInsert (3, 0, { bypassed: true }), "bypassing succeeds");
        check (server.fxRequests.at (-1)?.method === "PATCH"
                   && server.fxRequests.at (-1)?.body.bypassed === true,
               "bypass reaches PATCH /patches/3/fx/0");
        check (server.chain.latencySamples === 240, "a bypassed insert leaves the latency budget");

        const bypassed = renderApp (store, "/instrument?slot=3");

        check (bypassed.includes ('data-count="3" data-latency-samples="240"'),
               "the page drops the bypassed insert's latency");
        check (/data-index="0" data-type="compressor" data-kind="builtin" data-name="compressor" data-bypassed="true"/.test (bypassed),
               "the bypassed row is marked");

        check (await store.removeFxInsert (3, 1), "removing an insert succeeds");
        check (server.fxRequests.at (-1)?.method === "DELETE"
                   && server.fxRequests.at (-1)?.index === 1,
               "remove reaches DELETE /patches/3/fx/1");
        check (server.chain.inserts.length === 2, "the engine dropped the insert");

        const removed = renderApp (store, "/instrument?slot=3");

        check (removed.includes ('data-count="2"'), "the page renders the shortened chain");
        check (! removed.includes ('data-type="eq"'), "the removed insert is gone from the page");

        // --- The instrument source ------------------------------------------
        const plugin = PLUGIN_FIXTURE.plugins[0]!;

        check (await store.changeInstrumentSource (3, sourceRequestForPlugin (plugin)),
               "assigning an installed plug-in as the source succeeds");
        check (server.sourceRequests.length === 1
                   && server.sourceRequests[0]?.slot === 3
                   && server.sourceRequests[0]?.body.kind === "plugin"
                   && server.sourceRequests[0]?.body.path === "/usr/lib/vst3/TestPlugin.vst3",
               "POST /patches/3/source carries the bundle path");

        const pluginPage = renderApp (store, "/instrument?slot=3");

        check (pluginPage.includes ('data-testid="instrument-source" data-kind="plugin"'),
               "the page shows the new source kind");
        check (pluginPage.includes ('data-name="TestPlugin"'), "the page shows the plug-in name");
        check (pluginPage.includes ("native window on the engine desktop"),
               "a plug-in source offers its native window");
        check (! pluginPage.includes ("hosted headlessly"),
               "the page no longer claims plug-ins are hosted headlessly");
        check (/data-testid="source-save-state"[^>]*data-slot="3"/.test (pluginPage)
                   && ! /data-testid="source-save-state"[^>]*disabled/.test (pluginPage),
               "a plug-in source offers an enabled save-sound action");

        check (await store.savePatchSound (3),
               "the explicit Save sound to patch action calls the API");
        check (server.stateCaptureRequests.at (-1)?.slot === 3,
               "POST /patches/3/state/capture saves the running sound");
        check (store.getSnapshot ().capture?.sizeBytes === 4099,
               "the store keeps the engine's capture confirmation");

        const savedPage = renderApp (store, "/instrument?slot=3");

        check (savedPage.includes ('data-testid="source-save-status"')
                   && savedPage.includes ("Saved to patch · 4.0 kB"),
               "the page confirms the saved patch in place");
        check (savedPage.includes ('data-size-bytes="4099"'),
               "the confirmation publishes the stored blob size");

        check (await store.openInstrumentEditor (3),
               "the explicit Open editor action calls the API");
        check (server.editorRequests.at (-1)?.slot === 3
                   && server.editorRequests.at (-1)?.open === true,
               "POST /patches/3/editor opens the window");

        const openedPage = renderApp (store, "/instrument?slot=3");

        check (openedPage.includes ('data-testid="source-editor" data-kind="plugin" data-open="true"'),
               "the page reflects the open editor");
        check (openedPage.includes ('data-testid="source-close-editor"'),
               "an open editor offers a close control");

        check (await store.closeInstrumentEditor (3),
               "the explicit close action calls the API");
        check (server.editorRequests.at (-1)?.slot === 3
                   && server.editorRequests.at (-1)?.open === false,
               "DELETE /patches/3/editor closes the window");

        const closedPage = renderApp (store, "/instrument?slot=3");

        check (closedPage.includes ('data-testid="source-editor" data-kind="plugin" data-open="false"'),
               "the page reflects the closed editor");

        check (await store.changeInstrumentSource (3, sourceRequestForInitTone (880)),
               "switching back to the built-in tone succeeds");
        check (server.sourceRequests.at (-1)?.body.frequency === 880,
               "the tone frequency reaches the engine");

        const tonePage = renderApp (store, "/instrument?slot=3");

        check (tonePage.includes ('data-testid="instrument-source" data-kind="init-tone"'),
               "the page returns to the tone source");
        check (tonePage.includes ("there are no plug-in parameters to edit"),
               "a generated source explains that it has no editor parameters");

        // --- Following the controller ---------------------------------------
        const following = renderApp (store, "/instrument");

        check (following.includes ('data-testid="instrument-page" data-slot="3" data-armed="true" data-following="true"'),
               "the instrument page follows the armed slot");
        check (following.includes ("following the controller"),
               "the page says it is following the controller");

        check (await store.armSlot (5), "the controller arms another patch");
        check (renderApp (store, "/instrument").includes ('data-testid="instrument-page" data-slot="5"'),
               "the page follows the new armed slot");
        check (renderApp (store, "/instrument?slot=1").includes ('data-testid="instrument-page" data-slot="5"'),
               "a stale ?slot link does not override the controller");

        check (await store.armSlot (7), "the controller arms a third patch");
        check (renderApp (store, "/instrument?slot=1").includes ('data-testid="instrument-page" data-slot="7"'),
               "the page keeps following later arms");
        check (! renderApp (store, "/grid").includes ('data-testid="instrument-page"'),
               "arming never navigates the grid to the instrument page");
        check (! renderApp (store, "/plugins").includes ('data-testid="instrument-page"'),
               "arming never navigates the plugins page to the instrument page");

        // --- Failure surfaces ------------------------------------------------
        const failing = new AppStore (
            createControlApi ({ baseUrl: "http://127.0.0.1:1" }),
            { pollIntervalMs: 60_000 });

        check (! await failing.loadFxChain (3), "a chain failure is reported");
        check (failing.getSnapshot ().fxError !== null, "the page can show the chain error");
        check (! await failing.savePatchSound (3), "a save-sound failure is reported");
        check (failing.getSnapshot ().captureError !== null,
               "the page can show the save-sound error");
    }
    finally
    {
        await server.close ();
    }
}

function instrumentPluginEditorTest (): void
{
    const pluginPatch: ControlPatch = {
        slot: 2,
        id: "plugin-patch",
        name: "Plugin Patch",
        family: "keys",
        familyName: "Keys",
        instrument: {
            id: "com.instrumenthub.TestPlugin",
            name: "TestPlugin",
            kind: "plugin",
            pluginFormat: "VST3",
            pluginId: "com.instrumenthub.TestPlugin",
            pluginName: "TestPlugin",
            pluginManufacturer: "Instrument Hub",
            pluginVersion: "1.0.0",
        },
        volumeDb: -6,
        pan: -0.5,
        mute: false,
        solo: false,
        colorIndex: 4,
        warmState: "warm",
        warm: true,
        editorOpen: false,
        latencySamples: 96,
        latencyMs: 2,
    };

    const html = renderToStaticMarkup (
        <InstrumentPage
            patch={pluginPatch}
            armedSlot={2}
            following={false}
            chain={null}
            plugins={PLUGIN_FIXTURE.plugins}
            onCaptureState={() => {}}
        />);

    check (html.includes ('data-testid="instrument-source" data-kind="plugin"'),
           "a plugin patch renders its source");
    check (html.includes ('data-testid="source-open-editor"'), "a plugin patch offers its editor");
    check (html.includes ('data-testid="source-save-state"'), "a plugin patch offers an explicit sound save");
    check (! /data-testid="source-save-state"[^>]*disabled/.test (html),
           "the plugin sound save is enabled");
    check (html.includes ("native window on the engine desktop"),
           "the plugin editor offers the native window");
    check (html.includes ('data-testid="fx-loading"') && html.includes ('aria-busy="true"'),
           "a chain that has not loaded shows its loading state");
    check (html.includes ('data-testid="pan-value">L 50<'), "a left balance reads L 50");
    check (html.includes ('data-testid="fx-latency-total">latency unavailable<'),
           "a missing chain reports no latency instead of a zero");

    const empty = renderApp (
        new AppStore (createControlApi ({ baseUrl: "http://127.0.0.1:1" })),
        "/instrument");

    check (empty.includes ('data-testid="instrument-page" data-slot="-1"'),
           "the instrument page renders its empty state before the first state");
    check (empty.includes ('data-testid="instrument-name">Loading the set…<'),
           "the empty state explains that the set is loading");
}

function pluginsModelTest (): void
{
    const plugins = PLUGIN_FIXTURE.plugins;

    check (filterPlugins (plugins, "").length === 3, "an empty search lists every plug-in");
    check (filterPlugins (plugins, "   ").length === 3, "a blank search changes nothing");
    check (filterPlugins (plugins, "arturia").length === 1
               && filterPlugins (plugins, "arturia")[0]?.name === "Analog Lab V",
           "search matches the vendor");
    check (filterPlugins (plugins, "TESTPLUGIN").length === 1
               && filterPlugins (plugins, "TESTPLUGIN")[0]?.name === "TestPlugin",
           "search is case-insensitive");
    check (filterPlugins (plugins, "instrument hub test").length === 2,
           "every search term must match, narrowing the list");
    check (filterPlugins (plugins, "fx").length === 1
               && filterPlugins (plugins, "fx")[0]?.name === "TestReverb",
           "search matches the category");
    check (filterPlugins (plugins, "vst3 testreverb").length === 1,
           "search matches the format and bundle path");
    check (filterPlugins (plugins, "no-such-plugin").length === 0, "an unmatched search is empty");

    const assignments = pluginAssignmentsFromState (FIXTURE);

    check (assignments.length === 1, "only plugin-backed patches count as assignments");
    check (assignments[0]?.slot === 6 && assignments[0]?.patchName === "Celesta",
           "the assignment names its patch and slot");
    check (usedByPlugin (assignments, plugins[2]!).length === 1,
           "Analog Lab is used by its patch");
    check (usedByPlugin (assignments, plugins[0]!).length === 0,
           "an unassigned plug-in has no usage entries");
    check (pluginAssignments (FIXTURE.families.flatMap (family => family.patches)).length === 1,
           "assignments can be derived from a flat patch list");

    check (pluginKind (plugins[0]!) === "Instrument", "an instrument plug-in is labelled Instrument");
    check (pluginKind (plugins[1]!) === "Effect", "an effect plug-in is labelled Effect");
    check (pluginCountLabel (3, 3) === "3 plug-ins", "a full list shows only the total");
    check (pluginCountLabel (1, 3) === "1 of 3 plug-ins", "a filtered list shows both counts");
    check (pluginCountLabel (1, 1) === "1 plug-in", "the singular form is used for one plug-in");
    check (formatScanTime (0) === "never" && formatScanTime (Number.NaN) === "never",
           "a missing scan time reads as never");
    check (formatScanTime (1_700_000_000_000) !== "never",
           "a real scan timestamp is formatted");
}

async function pluginsPageTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        check (await store.refresh (), "the plugins fixture loads GET /state");
        check (await store.loadPlugins (), "GET /plugins returns the installed inventory");

        const inventory = store.getSnapshot ().plugins;

        check (inventory !== null && inventory.total === 3,
               "the inventory reports three installed plug-ins");

        const initial = renderApp (store, "/plugins");

        writeArtifact ("ui-plugins-snapshot.html", initial);

        check (initial.includes ('data-testid="plugins-page"'), "the /plugins route renders the page");
        check (countOf (initial, 'data-testid="plugin-row"') === 3,
               "the page lists every scanned plug-in");
        check (initial.includes ('data-testid="plugin-name">TestPlugin<'),
               "the page shows TestPlugin by name");
        check (initial.includes ('data-testid="plugin-name">TestReverb<'),
               "the page shows the scanned effect");
        check (initial.includes ('data-testid="plugin-name">Analog Lab V<'),
               "the page shows a third-party plug-in");
        check (initial.includes ('data-testid="plugin-vendor">Arturia<'),
               "the page shows the vendor");
        check (initial.includes ('data-testid="plugin-version">5.10.0<'),
               "the page shows the version");
        check (initial.includes ('data-testid="plugin-category">Instrument<'),
               "the page shows the category");
        check (initial.includes ('data-testid="plugin-format">VST3<'),
               "the page shows the format");
        check (initial.includes ("/usr/lib/vst3/TestPlugin.vst3"),
               "the page shows the bundle path");
        check (initial.includes ('data-instrument="true" data-effect="false">Instrument<'),
               "an instrument row is labelled as an instrument");
        check (initial.includes ('data-instrument="false" data-effect="true">Effect<'),
               "an effect row is labelled as an effect");

        check (countOf (initial, 'data-testid="plugin-used-by"') === 1,
               "only the assigned plug-in gets a used-by hint");
        check (initial.includes ('data-testid="plugin-used-by" data-count="1"'),
               "the used-by hint counts its patches");
        check (initial.includes ('data-patch="Celesta"')
                   && initial.includes ("slot 07 · Keys"),
               "the used-by hint names the patch, slot and family");
        check (countOf (initial, 'data-testid="plugin-unused"') === 2,
               "unassigned plug-ins are marked as unused");

        check (initial.includes ('data-testid="plugin-search"'), "the page has a search field");
        check (initial.includes ('data-testid="plugin-rescan"'), "the page has a rescan button");
        check (initial.includes ("3 plug-ins"), "the page reports the inventory size");
        check (! initial.includes ('data-testid="plugin-assign"'),
               "the page has no assignment control");
        check (! initial.includes ("<select"), "the page has no plug-in picker");
        check (initial.includes ("This page only reads the scan"),
               "the page says assignment belongs to the patch");

        // --- Search ---------------------------------------------------------
        const assignments = pluginAssignmentsFromState (store.getSnapshot ().state);
        const filtered = renderToStaticMarkup (
            <PluginsPage
                inventory={inventory}
                assignments={assignments}
                initialSearch="arturia"
            />);

        check (countOf (filtered, 'data-testid="plugin-row"') === 1,
               "search filters the visible rows");
        check (filtered.includes ('data-testid="plugin-name">Analog Lab V<'),
               "the matching plug-in stays visible");
        check (! filtered.includes ('data-testid="plugin-name">TestPlugin<'),
               "non-matching plug-ins are filtered out");
        check (filtered.includes ("1 of 3 plug-ins"), "the filtered count shows 1 of 3");

        const multiTerm = renderToStaticMarkup (
            <PluginsPage inventory={inventory} initialSearch="vst3 testreverb" />);

        check (countOf (multiTerm, 'data-testid="plugin-row"') === 1
                   && multiTerm.includes ('data-testid="plugin-name">TestReverb<'),
               "multi-term search matches the format and name together");

        const noMatch = renderToStaticMarkup (
            <PluginsPage inventory={inventory} initialSearch="no-such-plugin" />);

        check (noMatch.includes ('data-testid="plugin-empty"'), "an unmatched search shows an empty state");
        check (noMatch.includes ("No plug-ins match"), "the empty state repeats the query");
        check (countOf (noMatch, 'data-testid="plugin-row"') === 0, "an unmatched search lists no rows");

        // --- Used-by follows the patch assignment ---------------------------
        check (await store.changeInstrumentSource (3, sourceRequestForPlugin (PLUGIN_FIXTURE.plugins[0]!)),
               "assigning TestPlugin to a patch succeeds");

        const assigned = renderApp (store, "/plugins");

        check (countOf (assigned, 'data-testid="plugin-used-by"') === 2,
               "a second assignment adds a second used-by hint");
        check (/data-plugin-id="com.instrumenthub.TestPlugin"[^>]*data-used-by="1"/.test (assigned),
               "the TestPlugin row now reports one user");
        check (assigned.includes ('data-patch="Drawbar Organ"'),
               "the used-by hint follows the newly assigned patch");

        // --- Rescan ---------------------------------------------------------
        check (await store.rescanPlugins (), "POST /plugins/rescan starts a scan");
        check (server.pluginRequests.includes ("rescan"), "the rescan reached the engine");
        check (store.getSnapshot ().plugins?.scanning === true,
               "the store adopts the scanning state");

        const scanning = renderApp (store, "/plugins");

        check (scanning.includes ('data-testid="plugins-page" data-count="3" data-total="3" data-scanning="true"'),
               "the page reports that a scan is running");
        check (scanning.includes ('data-testid="plugin-scan-state" data-scanning="true"'),
               "the scan badge turns on");
        check (scanning.includes ("Scanning…"), "the scan badge is readable");
        check (/data-testid="plugin-rescan"[^>]*disabled/.test (scanning),
               "the rescan button waits for the running scan");

        check (await store.loadPlugins (), "the polled inventory read succeeds");
        check (store.getSnapshot ().plugins?.scanning === false, "the scan settles");
        check (store.getSnapshot ().plugins?.plugins.some (plugin => plugin.name === "TestDelay") === true,
               "the rescan picked up the newly installed plug-in");

        const rescanned = renderApp (store, "/plugins");

        check (countOf (rescanned, 'data-testid="plugin-row"') === 4,
               "the refreshed page lists the new plug-in");
        check (rescanned.includes ('data-testid="plugin-name">TestDelay<'),
               "the newly scanned plug-in is visible");
        check (rescanned.includes ("4 plug-ins"), "the count grows with the rescan");
        check (rescanned.includes ('data-scanning="false"'), "the page leaves its scanning state");

        // --- Failure surface -------------------------------------------------
        const failing = new AppStore (
            createControlApi ({ baseUrl: "http://127.0.0.1:1" }),
            { pollIntervalMs: 60_000 });

        check (! await failing.loadPlugins (), "a failed inventory read is reported");
        check (failing.getSnapshot ().pluginError !== null, "the store keeps the inventory error");

        const failureHtml = renderApp (failing, "/plugins");

        check (failureHtml.includes ('data-testid="plugins-page"'), "the page still renders on failure");
        check (failureHtml.includes ('data-testid="plugin-error"'), "the failure is shown");
        check (failureHtml.includes ('data-testid="plugin-retry"'), "the failure offers a retry");
        check (countOf (failureHtml, 'data-testid="plugin-row"') === 0,
               "a failed load lists no stale rows");
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
    routeTest ();
    await clickArmsAndRoutesTest ();
    await colourEditTest ();
    meterTest ();
    loadingTest ();
    await audioSettingsTest ();
    midiModelTest ();
    await midiPageTest ();
    await pairingTest ();
    qrTest ();
    remoteSnapshotTest ();
    remoteModelTest ();
    await remoteInteractionTest ();
    remoteBannerTest ();
    await remoteWakeLockTest ();
    remoteContrastTest ();
    await remoteLinkTest ();
    await remoteOptimisticTest ();
    await reconnectTest ();
    await backoffTest ();
    instrumentModelTest ();
    await instrumentPageTest ();
    instrumentPluginEditorTest ();
    pluginsModelTest ();
    await pluginsPageTest ();

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
