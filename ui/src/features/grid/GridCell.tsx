import type { ReactElement } from "react";

import { ARMED_COLOUR, colourForIndex } from "./colours";
import type { GridPatch } from "./model";
import { formatLatency, meterPercent, pad2 } from "./format";
import { PatchColourSwatch } from "./ColourSwatch";
import type { MeterSnapshot } from "./meterFrame";

export interface GridCellProps {
  readonly familyId: string;
  readonly familyName: string;
  readonly column: number;
  readonly patch: GridPatch | undefined;
  readonly armedSlot: number;
  readonly meters: MeterSnapshot;
  readonly onArm: ((slot: number) => void) | undefined;
  readonly onColourChange:
    ((slot: number, colorIndex: number) => void) | undefined;
}

function EmptyGridCell({
  familyId,
  familyName,
  column,
}: Pick<GridCellProps, "familyId" | "familyName" | "column">): ReactElement {
  return (
    <div
      className="grid__cell grid__cell--empty"
      role="gridcell"
      aria-disabled="true"
      aria-label={`${familyName}, empty patch ${String(column + 1)}`}
      data-testid="grid-cell"
      data-empty="true"
      data-family={familyId}
      data-column={column}
      data-colour-index={0}
      data-colour={colourForIndex(0)}
      data-fill={colourForIndex(0)}
      style={{
        background: `linear-gradient(150deg, ${colourForIndex(0)}33 0%, #14151c 70%)`,
      }}
    />
  );
}

export function GridCell({
  familyId,
  familyName,
  column,
  patch,
  armedSlot,
  meters,
  onArm,
  onColourChange,
}: GridCellProps): ReactElement {
  if (patch === undefined)
    return (
      <EmptyGridCell
        familyId={familyId}
        familyName={familyName}
        column={column}
      />
    );

  const slot = patch.slot;
  const armed = slot !== null && slot === armedSlot;
  const peak = slot === null ? 0 : (meters.slots[slot]?.peak ?? 0);
  const latencyMs = patch.latencyMs;
  const latencySamples = patch.latencySamples;
  const colour = colourForIndex(patch.colorIndex);
  const fill = armed ? ARMED_COLOUR : colour;

  return (
    <div
      className={`grid__cell${armed ? " grid__cell--armed" : ""}`}
      role="gridcell"
      data-testid="grid-cell"
      data-slot={slot ?? -1}
      data-armed={armed ? "true" : "false"}
      data-empty="false"
      data-family={familyId}
      data-column={column}
      data-colour-index={patch.colorIndex}
      data-colour={colour}
      data-fill={fill}
      data-warm-state={patch.warmState}
      style={{
        background: armed
          ? ARMED_COLOUR
          : `linear-gradient(150deg, ${colour}55 0%, #1c1d28 70%)`,
      }}
      aria-label={`${familyName}, ${patch.name}${armed ? ", armed" : ""}`}
    >
      <button
        className="grid__cell-main"
        type="button"
        data-testid="grid-cell-arm"
        aria-pressed={armed}
        onClick={
          onArm === undefined || slot === null
            ? undefined
            : () => {
                onArm(slot);
              }
        }
      >
        <span className="grid__cell-name">{patch.name}</span>

        <span className="grid__cell-foot">
          <span className="grid__cell-slot">
            {slot === null ? "—" : `S${pad2(slot + 1)}`}
          </span>
          {armed ? <span className="grid__cell-badge">Armed</span> : null}
        </span>

        {latencyMs !== undefined ? (
          <span
            className="grid__cell-latency"
            data-testid="slot-latency"
            data-slot={slot ?? -1}
            data-latency-samples={latencySamples ?? 0}
            data-latency-ms={latencyMs.toFixed(3)}
            title="Instrument + FX chain latency"
          >
            {formatLatency(latencyMs)}
          </span>
        ) : null}

        <span
          className="grid__cell-meter"
          data-testid="slot-meter"
          data-slot={slot ?? -1}
          data-peak={peak.toFixed(3)}
          aria-hidden="true"
        >
          <span
            className="grid__cell-meter-fill"
            style={{ width: `${String(meterPercent(peak))}%` }}
          />
        </span>
      </button>

      <PatchColourSwatch
        slot={slot}
        patchName={patch.name}
        colorIndex={patch.colorIndex}
        onCycle={
          onColourChange === undefined || slot === null
            ? undefined
            : (nextColorIndex) => {
                onColourChange(slot, nextColorIndex);
              }
        }
      />
    </div>
  );
}
