// Grid: cell rendering, arm/colour edits, meters and the loading state.

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import {
    EMPTY_METERS,
    GridView,
    MeterStore,
    PATCH_COLOURS,
    createGridModel,
} from "../../features/grid";
import { AppStore } from "../../state/store";
import { FakeMeterSocket } from "../fakes/meterSocket";
import { startFixtureServer } from "../fixtureServer";
import { GRID_METERS, GRID_MODEL } from "../fixtures";
import { check, countOf, renderApp, writeArtifact } from "../harness";

export async function gridSnapshotTest (): Promise<void>
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

export async function armTest (): Promise<void>
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

export async function colourEditTest (): Promise<void>
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

export function meterTest (): void
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

export function loadingTest (): void
{
    const store = new AppStore (createControlApi ({ baseUrl: "http://127.0.0.1:1" }));
    const html = renderApp (store);

    check (html.includes ('data-testid="grid-loading"'),
           "grid shows a loading skeleton before the first state");
    check (countOf (html, 'data-testid="grid-skeleton-cell"') === 64,
           "loading skeleton is 8×8");
    check (html.includes ('aria-busy="true"'), "loading grid is marked busy");
}

/** Full-app snapshot: the grid the fixture state renders. */
export function assertSnapshotGrid (html: string): void
{
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
}

/** Full-app snapshot: the armed highlight agrees with the fetched state. */
export function assertSnapshotArmed (html: string, armedSlot: number): void
{
    check (countOf (html, 'data-armed="true"') === (armedSlot >= 0 ? 1 : 0),
           `armed-slot highlight matches the fetched state (armedSlot ${String (armedSlot)})`);
}

/** Full-app snapshot: fixture patch names, tempo and transport. */
export function assertSnapshotGridFixture (html: string): void
{
    check (html.includes ("Drawbar Organ"), "snapshot shows the armed patch name");
    check (html.includes ("Studio Grand"), "snapshot shows the first slot name");
    check (html.includes ("128 BPM"), "snapshot shows the fetched tempo");
    check (html.includes ("Playing"), "snapshot shows the fetched transport state");
}
