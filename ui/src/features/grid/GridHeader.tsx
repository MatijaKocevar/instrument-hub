import type { ReactElement } from "react";

import { GridMeterStatus } from "./GridMeterStatus";
import type { GridLayout } from "./model";
import type { MeterSnapshot } from "./meterFrame";

export interface GridHeaderProps {
  readonly meters: MeterSnapshot;
  readonly layout?: GridLayout;
}

export function GridHeader({
  meters,
  layout = "families",
}: GridHeaderProps): ReactElement {
  const tracks = layout === "tracks";

  return (
    <header className="grid__header">
      <div className="grid__heading">
        <h2 className="grid__title">Instrument grid</h2>
        <p className="grid__subtitle">
          {tracks
            ? "8 tracks × 8 rows · tap a track to select and open it · FX and volume live in the grid"
            : "8 families × 8 patches · tap a patch to arm and open it · tap a swatch to recolour"}
        </p>
      </div>

      <GridMeterStatus meters={meters} />
    </header>
  );
}
