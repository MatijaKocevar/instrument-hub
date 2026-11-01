import type { ReactElement } from "react";

import { GridHeader } from "./GridHeader";
import { GridTable } from "./GridTable";
import { LoadingGrid } from "./LoadingGrid";
import type { GridModel } from "./model";
import { EMPTY_METERS, type MeterSnapshot } from "./meterFrame";
import "./grid.css";

export interface GridViewProps {
  readonly model: GridModel;
  readonly meters?: MeterSnapshot;
  readonly loading?: boolean;
  readonly onArm?: (slot: number) => void;
  readonly onColourChange?: (slot: number, colorIndex: number) => void;
  readonly onFamilyColourChange?: (
    familyId: string,
    slots: readonly number[],
    colorIndex: number,
  ) => void;
}

export function GridView({
  model,
  meters = EMPTY_METERS,
  loading = false,
  onArm,
  onColourChange,
  onFamilyColourChange,
}: GridViewProps): ReactElement {
  return (
    <section
      className="grid"
      aria-label="Instrument grid"
      data-testid="instrument-grid"
      data-armed-slot={model.armedSlot}
      data-loading={loading ? "true" : "false"}
    >
      <GridHeader meters={meters} />

      {loading ? (
        <LoadingGrid />
      ) : model.families.length === 0 ? (
        <div className="grid__empty" data-testid="grid-empty">
          No instruments in this set yet.
        </div>
      ) : (
        <GridTable
          model={model}
          meters={meters}
          onArm={onArm}
          onColourChange={onColourChange}
          onFamilyColourChange={onFamilyColourChange}
        />
      )}
    </section>
  );
}
