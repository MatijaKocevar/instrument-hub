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
import { createServer, type Server } from "node:http";
import type { AddressInfo } from "node:net";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import App from "../App";
import { createControlApi, type ControlState } from "../api/client";
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
import { AppStoreProvider } from "../state/AppStoreContext";
import { AppStore } from "../state/store";

const FIXTURE: ControlState = {
    schemaVersion: 1,
    application: { name: "Instrument Hub", version: "0.1.0" },
    armedSlot: 3,
    uptimeMs: 128_000,
    slots: [
        { index: 0, name: "Studio Grand", armed: false },
        { index: 1, name: "Warm Pad", armed: false },
        { index: 2, name: "Electric Keys", armed: false },
        { index: 3, name: "Sub Bass", armed: true },
        { index: 4, name: "Drum Kit", armed: false },
        { index: 5, name: "Analog Brass", armed: false },
        { index: 6, name: "Vocal Chop", armed: false },
        { index: 7, name: "Empty Slot", armed: false },
    ],
    settings: { masterGain: 0.82, inputGain: 1.0, midiChannel: 2, metronome: true },
    transport: { playing: true, bpm: 128, ppq: 16.5, positionSamples: 384_512 },
};

/** Eight families for the feature-level grid snapshot. */
const GRID_FAMILIES: GridFamily[] = [
    {
        id: "keys",
        name: "Keys",
        patches: [
            { id: "piano", name: "Grand Piano", slot: 0 },
            { id: "strings", name: "Warm Strings", slot: 1 },
        ],
    },
    { id: "drums", name: "Drums", patches: [{ id: "kit", name: "Acoustic Kit", slot: 2 }] },
    { id: "bass", name: "Bass", patches: [{ id: "sub", name: "Sub Bass", slot: 3 }] },
    { id: "pads", name: "Pads", patches: [{ id: "air", name: "Air Pad", slot: 4 }] },
    { id: "lead", name: "Lead", patches: [{ id: "saw", name: "Saw Lead", slot: 5 }] },
    { id: "fx", name: "FX", patches: [{ id: "riser", name: "Riser", slot: 6 }] },
    { id: "vocal", name: "Vocal", patches: [{ id: "chop", name: "Vocal Chop", slot: 7 }] },
    { id: "perc", name: "Percussion", patches: [{ id: "shaker", name: "Shaker", slot: null }] },
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
    const armRequests: { slot: number | null }[] = [];
    let stateRequests = 0;

    const server = createServer ((request, response) =>
    {
        response.setHeader ("content-type", "application/json");

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

        response.statusCode = 404;
        response.end (JSON.stringify ({ error: "not found" }));
    });

    const port = await listen (server);

    return {
        baseUrl: `http://127.0.0.1:${String (port)}`,
        get stateRequests () { return stateRequests; },
        get armRequests () { return armRequests; },
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
    check (html.includes ('data-phase="ready"'), "connection is ready");
    check (html.includes ("Connected"), "snapshot shows the connected status");
    check (html.includes ("Instrument Hub"), "snapshot shows the application name");
    check (html.includes ("0.1.0"), "snapshot shows the application version");
    check (html.includes ('data-testid="armed-slot"'), "snapshot shows the armed-slot widget");
    check (html.includes ('data-testid="connection-status"'), "snapshot shows the connection widget");
    check (html.includes ('data-testid="transport-bpm"'), "snapshot shows the transport widget");
    check (html.includes ('data-testid="instrument-grid"'), "snapshot shows the instrument grid");

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
        getState: async (): Promise<ControlState> =>
        {
            throw new Error ("connection refused");
        },
        armSlot: async (): Promise<ControlState> =>
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

async function main (): Promise<void>
{
    await snapshotTest ();
    await gridSnapshotTest ();
    await armTest ();
    meterTest ();
    loadingTest ();
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
