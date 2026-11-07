import { renderToStaticMarkup } from "react-dom/server";

import { GridView, gridFromState } from "../../features/grid";
import { FIXTURE } from "../fixtures";
import { check, countOf, writeArtifact } from "../harness";

export function legacyGridTest(): void {
  const model = gridFromState(FIXTURE);
  const html = renderToStaticMarkup(
    <GridView model={model} onArm={() => {}} />,
  );

  writeArtifact("ui-legacy-grid-snapshot.html", html);

  check(
    model.layout === "families",
    "a legacy project stays on the family grid",
  );
  check(
    countOf(html, 'data-testid="family"') === 8,
    "the legacy grid still renders eight families",
  );
  check(
    countOf(html, 'data-testid="grid-cell"') === 64,
    "the legacy grid still renders sixty-four patch cells",
  );
  check(
    countOf(html, 'data-testid="track-grid-table"') === 0 &&
      countOf(html, 'data-testid="track-fader"') === 0,
    "the legacy grid has no track channel strips",
  );
  check(
    html.includes("Studio Grand") && html.includes("Warm Pad"),
    "legacy patch names are unchanged",
  );
}
