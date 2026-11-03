import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import { GridView, gridFromState } from "../../features/grid";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { FIXTURE, TRACK_FIXTURE } from "../fixtures";
import { check, countOf, renderApp, writeArtifact } from "../harness";

const TRACK_ROLES = [
  "track",
  "fx",
  "fx",
  "volume",
  "volume",
  "volume",
  "volume",
  "volume",
] as const;

function rowRoles(html: string): string[] {
  return html
    .split('data-testid="track-row"')
    .slice(1)
    .map((markup) => /data-role="([^"]+)"/.exec(markup)?.[1] ?? "");
}

function rowNumbers(html: string): number[] {
  return html
    .split('data-testid="track-row"')
    .slice(1)
    .map((markup) => Number(/data-row="(\d+)"/.exec(markup)?.[1] ?? -1));
}

export function trackGridModelTest(): void {
  const model = gridFromState(TRACK_FIXTURE);
  const html = renderToStaticMarkup(
    <GridView
      model={model}
      onSelectTrack={() => {}}
      onToggleTrackFx={() => {}}
      onSetTrackVolume={() => {}}
    />,
  );

  writeArtifact("ui-track-grid-snapshot.html", html);

  check(model.layout === "tracks", "a track project builds a track grid model");
  check(
    countOf(html, 'data-testid="track-grid-table"') === 1,
    "the track grid table renders",
  );
  check(
    countOf(html, 'data-testid="family"') === 0,
    "the track grid does not render the legacy family rows",
  );
  check(
    countOf(html, 'data-testid="track-column"') === 8,
    "the track grid shows eight track columns",
  );
  check(
    countOf(html, 'data-testid="track-row"') === 8,
    "the track grid shows eight hardware rows",
  );
  check(
    rowNumbers(html).join(",") === "7,6,5,4,3,2,1,0",
    "track rows are numbered top-down from row 7 to row 0",
  );
  check(
    rowRoles(html).join(",") === TRACK_ROLES.join(","),
    "row 7 selects a track, rows 6/5 are hot FX and rows 4-0 are volume steps",
  );
  check(
    countOf(html, 'data-testid="track-cell"') === 64,
    "the track grid is eight tracks by eight rows",
  );
  check(
    countOf(html, 'data-testid="track-select"') === 8,
    "every column has a track selector",
  );
  check(
    countOf(html, 'data-testid="track-hot-fx"') === 16,
    "every column has two hot FX toggles",
  );
  check(
    countOf(html, 'data-testid="track-volume"') === 40,
    "every column has five volume steps",
  );
  check(
    countOf(html, 'data-testid="track-select" data-track-index="2"') === 1 &&
      html.includes(
        'data-testid="track-cell" data-track-index="2" data-track-id="track-3" data-row="7" data-role="track" data-slot="2" data-selected="true"',
      ),
    "the selected track is marked on row 7",
  );
  check(
    countOf(html, 'data-selected="true"') === 1,
    "exactly one track is selected",
  );
  check(
    html.includes('data-hot="0" data-assigned="true"') &&
      html.includes(">EQ<") &&
      html.includes(">Compressor<"),
    "assigned hot FX show their insert names",
  );
  check(
    countOf(html, 'data-assigned="false"') === 20,
    "unassigned hot FX stay dark",
  );
  check(
    countOf(html, 'data-over="true"') === 2 && html.includes(">+6 dB<"),
    "the over step renders when a track is above 100%",
  );
  check(
    html.includes("Drawbar Organ") && html.includes("Analog Lab V"),
    "track instruments are visible in the selector",
  );

  const legacy = gridFromState(FIXTURE);
  const legacyHtml = renderToStaticMarkup(
    <GridView model={legacy} onArm={() => {}} />,
  );

  check(
    legacy.layout === "families",
    "a legacy project stays on the family grid",
  );
  check(
    countOf(legacyHtml, 'data-testid="family"') === 8 &&
      countOf(legacyHtml, 'data-testid="track-grid-table"') === 0,
    "legacy projects still render the family grid",
  );
}

export async function trackGridInteractionTest(): Promise<void> {
  const server = await startFixtureServer({ layout: "tracks" });

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    check(await store.refresh(), "the track fixture loads GET /state");

    const initial = renderApp(store, "/grid");

    check(
      initial.includes('data-testid="instrument-grid" data-layout="tracks"'),
      "the grid follows the track layout from the API",
    );
    check(
      initial.includes(
        'data-testid="track-cell" data-track-index="2" data-track-id="track-3" data-row="7" data-role="track" data-slot="2" data-selected="true"',
      ),
      "the armed track starts selected",
    );
    check(
      initial.includes('data-testid="track-column"') &&
        countOf(initial, 'data-testid="track-column"') === 8,
      "the live track grid renders eight columns",
    );

    check(await store.selectTrack(5), "selecting a track succeeds");
    check(
      server.trackRequests.at(-1)?.kind === "select" &&
        server.trackRequests.at(-1)?.index === 5,
      "track selection reaches POST /tracks/5/select",
    );
    check(
      store.getSnapshot().state?.armedSlot === 5,
      "selecting a track arms its slot",
    );

    const selected = renderApp(store, "/grid");

    check(
      selected.includes(
        'data-testid="track-cell" data-track-index="5" data-track-id="track-6" data-row="7" data-role="track" data-slot="5" data-selected="true"',
      ),
      "the grid selection follows the API",
    );

    check(await store.toggleTrackFx(2, 0), "toggling a hot FX succeeds");
    check(
      server.trackRequests.at(-1)?.kind === "toggle" &&
        server.trackRequests.at(-1)?.index === 2 &&
        server.trackRequests.at(-1)?.hot === 0,
      "the hot FX toggle reaches POST /tracks/2/fx/0/toggle",
    );
    check(
      renderApp(store, "/grid").includes(
        'data-testid="track-hot-fx" data-track-index="2" data-hot="0" data-assigned="true" data-bypassed="true"',
      ),
      "the grid follows the hot FX bypass state",
    );

    check(await store.setTrackVolume(2, 5), "setting the over step succeeds");
    check(
      server.trackRequests.at(-1)?.kind === "volume" &&
        server.trackRequests.at(-1)?.index === 2 &&
        server.trackRequests.at(-1)?.step === 5,
      "the volume step reaches POST /tracks/2/volume",
    );
    check(
      renderApp(store, "/grid").includes(
        'data-testid="track-volume" data-track-index="2" data-step="4" data-active="true" data-over="true"',
      ),
      "the grid shows the over state on the 100% row",
    );

    check(await store.selectTrack(2), "reselecting the edited track succeeds");
    check(await store.loadFxChain(2), "the selected track chain loads");

    const instrument = renderApp(store, "/instrument");

    check(
      instrument.includes(
        'data-testid="instrument-page" data-slot="2" data-armed="true"',
      ),
      "the instrument page opens the selected track's slot",
    );
    check(
      instrument.includes('data-testid="instrument-name">Track 3<') &&
        instrument.includes('data-name="Drawbar Organ"'),
      "the selected track's instrument reaches the instrument page",
    );
    check(
      instrument.includes('data-testid="source-change"'),
      "the selected track can choose another instrument",
    );
    check(
      instrument.includes('data-testid="track-hot-summary"'),
      "the instrument page names the two hot FX",
    );
    check(
      countOf(instrument, 'data-testid="fx-hot-1"') === 2 &&
        countOf(instrument, 'data-testid="fx-hot-2"') === 2,
      "every insert can be marked as hot FX 1 or 2",
    );

    check(
      await store.assignTrackHotFx(2, 0, "compressor"),
      "marking an insert as hot FX succeeds",
    );
    check(
      server.trackRequests.at(-1)?.kind === "assign" &&
        server.trackRequests.at(-1)?.index === 2 &&
        server.trackRequests.at(-1)?.hot === 0 &&
        server.trackRequests.at(-1)?.insertId === "compressor",
      "the hot assignment reaches POST /tracks/2/fx/0/assign",
    );

    const assigned = renderApp(store, "/grid");

    check(
      assigned.includes(
        'data-testid="track-hot-fx" data-track-index="2" data-hot="0" data-assigned="true"',
      ) &&
        assigned.includes('data-name="Compressor"') &&
        assigned.includes(">Compressor<"),
      "the grid follows the new hot FX assignment",
    );
    check(
      assigned.includes(
        'data-testid="track-hot-fx" data-track-index="2" data-hot="1" data-assigned="false"',
      ),
      "assigning the other insert clears the previous hot slot",
    );

    const quietTrack = renderApp(store, "/instrument");

    check(
      quietTrack.includes(
        'data-testid="track-hot-summary" data-track-index="2" data-hot-1="Compressor" data-hot-2="Unassigned"',
      ),
      "the instrument page follows the hot FX assignment",
    );
  } finally {
    await server.close();
  }
}
