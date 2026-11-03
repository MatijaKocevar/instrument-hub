import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import { EMPTY_METERS, GridView, createGridModel } from "../../features/grid";
import { GRID_FAMILIES, GRID_METERS, GRID_MODEL } from "../fixtures";
import { renderedGridRows } from "../gridRows";
import { check, countOf, writeArtifact } from "../harness";

export async function gridSnapshotTest(): Promise<void> {
  const html = renderToStaticMarkup(
    <GridView model={GRID_MODEL} meters={GRID_METERS} onArm={() => {}} />,
  );

  writeArtifact("ui-grid-snapshot.html", html);

  check(html.length > 2000, "grid snapshot is non-blank");
  check(
    countOf(html, 'data-testid="family"') === 8,
    "grid snapshot shows 8 families",
  );
  check(
    countOf(html, 'data-testid="grid-column"') === 8,
    "grid snapshot shows 8 patch columns",
  );
  check(
    countOf(html, 'data-testid="grid-cell"') === 64,
    "grid snapshot is 8×8 capable",
  );
  check(
    countOf(html, 'data-empty="true"') === 55,
    "unused patches render as disabled cells",
  );
  check(
    countOf(html, 'data-testid="slot-meter"') === 9,
    "each assigned patch owns a meter",
  );
  check(
    countOf(html, 'data-testid="slot-latency"') === 8,
    "each assigned patch shows its chain latency",
  );
  check(
    html.includes(
      'data-testid="slot-latency" data-slot="3" data-latency-samples="240" data-latency-ms="5.000"',
    ),
    "the 5 ms chain latency is published into the grid markup",
  );
  check(
    html.includes(">5.00 ms<"),
    "the grid renders the latency in milliseconds",
  );
  check(html.includes(">Keys<"), "grid names the Keys family");
  check(
    html.includes("Grand Piano") && html.includes("Warm Strings"),
    "a family renders more than one patch",
  );
  check(
    countOf(html, 'data-armed="true"') === 1,
    "exactly one grid cell is armed",
  );
  check(
    html.includes('data-slot="2" data-armed="true"'),
    "the armed cell is highlighted",
  );
  check(
    countOf(html, 'data-fill="') === 64,
    "every grid cell carries a colour",
  );
  check(
    countOf(html, 'data-fill="#ff1f2f"') === 1,
    "the armed cell is bright red",
  );
  check(
    html.includes('data-slot="4" data-armed="false" data-empty="false"') &&
      html.includes('data-colour-index="3"'),
    "an unarmed cell keeps its patch colour",
  );
  check(
    html.includes('data-testid="slot-meter" data-slot="2" data-peak="0.500"'),
    "per-slot meter carries the latest peak",
  );
  check(
    html.includes('data-testid="master-meter" data-peak="0.310"'),
    "master meter carries the latest peak",
  );
  check(
    html.includes('data-testid="meters-status" data-connected="true"'),
    "grid reports live meters",
  );
  check(
    html.includes('data-slot="-1" data-armed="false"'),
    "an unassigned patch still renders a cell",
  );

  const rows = renderedGridRows(html);

  check(rows.length === 8, "the grid snapshot renders eight hardware rows");
  check(
    rows[0]?.family === "perc" && rows[0]?.row === 7,
    "the first rendered row is the last family, on hardware row 8",
  );
  check(
    rows[7]?.family === "keys" && rows[7]?.row === 0,
    "the last rendered row is the first family, on hardware row 1",
  );
  check(
    rows.map((row) => row.family).join(",") ===
      [...GRID_FAMILIES]
        .map((family) => family.id)
        .reverse()
        .join(","),
    "the fixture rows render top-down",
  );
  check(
    html.includes('<span class="grid__family-index">08</span>'),
    "the top rendered row is numbered 8, matching the Launchpad",
  );

  const loadingHtml = renderToStaticMarkup(
    <GridView model={createGridModel([], -1)} meters={EMPTY_METERS} loading />,
  );

  writeArtifact("ui-grid-loading.html", loadingHtml);

  check(
    loadingHtml.includes('data-testid="grid-loading"'),
    "loading state renders a skeleton",
  );
  check(
    loadingHtml.includes('aria-busy="true"'),
    "loading grid is marked busy",
  );
  check(
    countOf(loadingHtml, 'data-testid="grid-skeleton-cell"') === 64,
    "loading skeleton keeps the 8×8 layout",
  );
  check(
    countOf(loadingHtml, 'data-testid="family"') === 0,
    "loading state does not show stale families",
  );

  const css = readFileSync(
    fileURLToPath(new URL("../src/features/grid/grid.css", import.meta.url)),
    "utf8",
  );

  check(
    /min-width:\s*44px/.test(css) && /touch-action:\s*manipulation/.test(css),
    "grid cells are touch-friendly (44 px targets, manipulation gestures)",
  );
  check(
    /\.grid__cell--armed\s*\{[^}]*background:\s*#ff1f2f/s.test(css),
    "the armed cell's CSS background is bright red",
  );
}

export function assertSnapshotGrid(html: string): void {
  check(
    countOf(html, 'data-testid="family"') === 8,
    "snapshot shows 8 families",
  );
  check(
    countOf(html, 'data-testid="grid-column"') === 8,
    "snapshot shows 8 patch columns",
  );
  check(
    countOf(html, 'data-testid="grid-cell"') === 64,
    "snapshot shows a full 8×8 grid",
  );
  check(
    countOf(html, 'data-fill="') === 64,
    "every one of the 64 cells carries a colour",
  );
  check(
    countOf(html, 'data-testid="slot-meter"') === 64,
    "every patch has a level meter",
  );
  check(
    countOf(html, 'data-testid="slot-latency"') === 64,
    "every patch shows its chain latency",
  );
  check(
    html.includes(
      'data-testid="slot-latency" data-slot="3" data-latency-samples="240" data-latency-ms="5.000"',
    ),
    "the API chain latency reaches the grid",
  );
  check(countOf(html, 'data-armed="true"') === 1, "exactly one cell is armed");
  check(
    html.includes('data-slot="3" data-armed="true"'),
    "the armed cell is slot 3",
  );
  check(
    countOf(html, 'data-fill="#ff1f2f"') === 1,
    "the armed cell is painted bright red",
  );

  const rows = renderedGridRows(html);

  check(rows.length === 8, "the snapshot renders eight hardware rows");
  check(
    rows[0]?.row === 7 &&
      rows[0]?.slots.join(",") === "56,57,58,59,60,61,62,63",
    "the top rendered row holds slots 57–64",
  );
  check(
    rows[7]?.row === 0 && rows[7]?.slots.join(",") === "0,1,2,3,4,5,6,7",
    "the bottom rendered row holds slots 1–8",
  );
}

export function assertSnapshotArmed(html: string, armedSlot: number): void {
  check(
    countOf(html, 'data-armed="true"') === (armedSlot >= 0 ? 1 : 0),
    `armed-slot highlight matches the fetched state (armedSlot ${String(armedSlot)})`,
  );
}

export function assertSnapshotGridFixture(html: string): void {
  check(html.includes("Drawbar Organ"), "snapshot shows the armed patch name");
  check(html.includes("Studio Grand"), "snapshot shows the first slot name");
  check(html.includes("128 BPM"), "snapshot shows the fetched tempo");
  check(html.includes("Playing"), "snapshot shows the fetched transport state");
}
