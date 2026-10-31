// Headless snapshot test for the React UI.
//
// Runs the real app under `react-dom/server`: no browser, no Chrome, no
// WebView. The state is fetched over real HTTP from a fixture server that
// speaks the control API, then the rendered markup is asserted for the
// expected widget count and content and written to tests/artifacts.
//
// Run with: pnpm test:snapshot

import { mkdirSync, writeFileSync } from "node:fs";
import { createServer, type Server } from "node:http";
import type { AddressInfo } from "node:net";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import App from "../App";
import { createControlApi, type ControlState } from "../api/client";
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

interface FixtureServer
{
    baseUrl: string;
    requests: number;
    close: () => Promise<void>;
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

/**
    Serve `GET /state` from a fixture.

    `failFirst` makes the first N requests fail with HTTP 503 so the test can
    observe the store reconnecting.
*/
async function startFixtureServer (failFirst = 0): Promise<FixtureServer>
{
    let requests = 0;

    const server = createServer ((request, response) =>
    {
        requests += 1;

        response.setHeader ("content-type", "application/json");

        if (request.method !== "GET" || request.url !== "/state")
        {
            response.statusCode = 404;
            response.end (JSON.stringify ({ error: "not found" }));
            return;
        }

        if (requests <= failFirst)
        {
            response.statusCode = 503;
            response.end (JSON.stringify ({ error: "engine is warming up" }));
            return;
        }

        response.statusCode = 200;
        response.end (JSON.stringify (FIXTURE));
    });

    const port = await listen (server);

    return {
        baseUrl: `http://127.0.0.1:${String (port)}`,
        get requests () { return requests; },
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

    const server = await startFixtureServer (0);

    try
    {
        await renderSnapshot (server.baseUrl, true);
        check (server.requests === 1, "exactly one state request was made");
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

    const html = renderToStaticMarkup (
        <AppStoreProvider store={store}>
            <App />
        </AppStoreProvider>);

    const artifactName = fromFixture ? "ui-state-snapshot.html" : "ui-state-live.html";
    const artifactPath = fileURLToPath (
        new URL (`../../tests/artifacts/${artifactName}`, import.meta.url));

    mkdirSync (fileURLToPath (new URL ("../../tests/artifacts/", import.meta.url)), { recursive: true });
    writeFileSync (artifactPath, html);

    console.log (`snapshot: wrote ${String (html.length)} bytes to ${artifactPath}`);

    check (html.length > 2000, "snapshot is non-blank");
    check (countOf (html, 'data-testid="slot"') === 8, "snapshot contains 8 slots");
    check (html.includes ('data-phase="ready"'), "connection is ready");
    check (html.includes ("Connected"), "snapshot shows the connected status");
    check (html.includes ("Instrument Hub"), "snapshot shows the application name");
    check (html.includes ("0.1.0"), "snapshot shows the application version");
    check (html.includes ('data-testid="armed-slot"'), "snapshot shows the armed-slot widget");
    check (html.includes ('data-testid="connection-status"'), "snapshot shows the connection widget");
    check (html.includes ('data-testid="transport-bpm"'), "snapshot shows the transport widget");

    // The markup must agree with the state the API actually returned.
    const armedSlot = store.getSnapshot ().state?.armedSlot ?? -1;
    check (countOf (html, 'data-armed="true"') === (armedSlot >= 0 ? 1 : 0),
           `armed-slot highlight matches the fetched state (armedSlot ${String (armedSlot)})`);
    check ((armedSlot < 0) === html.includes ("armed none"),
           "footer reports the fetched arm state");

    if (fromFixture)
    {
        check (countOf (html, 'data-armed="true"') === 1, "exactly one slot is armed");
        check (html.includes ("Sub Bass"), "snapshot shows the armed slot name");
        check (html.includes ("Studio Grand"), "snapshot shows the first slot name");
        check (html.includes ("128 BPM"), "snapshot shows the fetched tempo");
        check (html.includes ("Playing"), "snapshot shows the fetched transport state");
    }
}

async function reconnectTest (): Promise<void>
{
    const server = await startFixtureServer (2);

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
        check (server.requests >= 3, "reconnect retried the request");
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
