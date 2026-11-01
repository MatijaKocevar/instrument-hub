import type { ReactElement } from "react";

import { GridMeterStatus } from "./GridMeterStatus";
import type { MeterSnapshot } from "./meterFrame";

export interface GridHeaderProps {
  readonly meters: MeterSnapshot;
}

export function GridHeader({ meters }: GridHeaderProps): ReactElement {
  return (
    <header className="grid__header">
      <div className="grid__heading">
        <h2 className="grid__title">Instrument grid</h2>
        <p className="grid__subtitle">
          8 families × 8 patches · tap a patch to arm and open it · tap a swatch
          to recolour
        </p>
      </div>

      <GridMeterStatus meters={meters} />
    </header>
  );
}
