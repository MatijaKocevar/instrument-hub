import type { ReactElement } from "react";

import {
    ARMED_COLOUR,
    colourForIndex,
    GRID_COLUMNS,
    GRID_ROWS,
    nextColourIndex,
    type GridFamily,
    type GridModel,
} from "./model";
import { EMPTY_METERS, type MeterSnapshot } from "./meters";
import "./grid.css";

export interface GridViewProps
{
    /** Families and the armed slot to highlight. */
    readonly model: GridModel;
    /** Latest meter frame; zeros when omitted. */
    readonly meters?: MeterSnapshot;
    /** Show the 8×8 skeleton while the first state arrives. */
    readonly loading?: boolean;
    /** Called with the engine slot when a patch cell is pressed. */
    readonly onArm?: (slot: number) => void;
    /** Called when the little colour swatch on a cell is pressed. */
    readonly onColourChange?: (slot: number, colorIndex: number) => void;
    /** Called when a family's colour swatch is pressed. */
    readonly onFamilyColourChange?: (familyId: string, slots: readonly number[], colorIndex: number) => void;
}

function meterPercent (peak: number): number
{
    const clamped = Math.min (1, Math.max (0, Number.isFinite (peak) ? peak : 0));

    // A square-root scale gives quiet material visible movement without a
    // logarithmic lookup table in the render path.
    return Math.round (Math.sqrt (clamped) * 100);
}

function pad2 (value: number): string
{
    return value.toString ().padStart (2, "0");
}

/** A compact, fixed-width chain latency readout for a patch cell. */
function formatLatency (milliseconds: number): string
{
    const clamped = Math.max (0, Number.isFinite (milliseconds) ? milliseconds : 0);

    return `${clamped.toFixed (2)} ms`;
}

function familyColour (family: GridFamily): number
{
    return family.patches[0]?.colorIndex ?? 0;
}

function familySlots (family: GridFamily): number[]
{
    return family.patches
        .map (patch => patch.slot)
        .filter ((slot): slot is number => slot !== null);
}

function LoadingGrid (): ReactElement
{
    return (
        <div className="grid__scroll">
            <div
                className="grid__loading"
                role="status"
                aria-busy="true"
                aria-label="Loading instruments"
                data-testid="grid-loading"
            >
                {Array.from ({ length: GRID_ROWS }, (_, row) => (
                    <div className="grid__loading-row" key={`loading-row-${String (row)}`}>
                        <span className="grid__loading-family" data-testid="grid-loading-family" />
                        {Array.from ({ length: GRID_COLUMNS }, (_, column) => (
                            <span
                                className="grid__loading-cell"
                                key={`loading-cell-${String (row)}-${String (column)}`}
                                data-testid="grid-skeleton-cell"
                            />
                        ))}
                    </div>
                ))}
            </div>
        </div>
    );
}

/**
    The main instrument grid: families down, patches across.

    Every cell is at least a 44 px touch target and carries its resolved patch
    colour. The armed cell is painted bright red. Pressing the cell body arms
    the patch; pressing its corner swatch cycles the patch colour without
    arming, and the family swatch recolours the whole row.
*/
export function GridView ({
    model,
    meters = EMPTY_METERS,
    loading = false,
    onArm,
    onColourChange,
    onFamilyColourChange,
}: GridViewProps): ReactElement
{
    return (
        <section
            className="grid"
            aria-label="Instrument grid"
            data-testid="instrument-grid"
            data-armed-slot={model.armedSlot}
            data-loading={loading ? "true" : "false"}
        >
            <header className="grid__header">
                <div className="grid__heading">
                    <h2 className="grid__title">Instrument grid</h2>
                    <p className="grid__subtitle">
                        8 families × 8 patches · tap a patch to arm and open it · tap a swatch to recolour
                    </p>
                </div>

                <div
                    className={`grid__meters-status${meters.connected ? " grid__meters-status--live" : ""}`}
                    data-testid="meters-status"
                    data-connected={meters.connected ? "true" : "false"}
                >
                    <span className="grid__meters-label">
                        {meters.connected ? "Meters live" : "Meters idle"}
                    </span>
                    <span
                        className="grid__master-meter"
                        data-testid="master-meter"
                        data-peak={meters.masterPeak.toFixed (3)}
                        aria-hidden="true"
                    >
                        <span
                            className="grid__master-meter-fill"
                            style={{ width: `${String (meterPercent (meters.masterPeak))}%` }}
                        />
                    </span>
                </div>
            </header>

            {loading
                ? <LoadingGrid />
                : model.families.length === 0
                    ? <div className="grid__empty" data-testid="grid-empty">
                          No instruments in this set yet.
                      </div>
                    : <div className="grid__scroll">
                          <div
                              className="grid__table"
                              role="grid"
                              aria-label="Instrument families and patches"
                              aria-rowcount={model.families.length + 1}
                              aria-colcount={GRID_COLUMNS + 1}
                              data-testid="grid-table"
                          >
                              <div className="grid__head" role="row" data-testid="grid-head">
                                  <span className="grid__corner" role="columnheader">
                                      Families
                                  </span>
                                  {Array.from ({ length: GRID_COLUMNS }, (_, column) => (
                                      <span
                                          className="grid__column"
                                          role="columnheader"
                                          key={`column-${String (column)}`}
                                          data-testid="grid-column"
                                      >
                                          {pad2 (column + 1)}
                                      </span>
                                  ))}
                              </div>

                              {model.families.map ((family, row) => {
                                  const colour = familyColour (family);
                                  const slots = familySlots (family);

                                  return (
                                      <div
                                          className="grid__row"
                                          role="row"
                                          key={family.id}
                                          data-testid="family"
                                          data-family={family.id}
                                          data-row={row}
                                          data-colour-index={colour}
                                          data-colour={colourForIndex (colour)}
                                      >
                                          <div
                                              className="grid__family"
                                              role="rowheader"
                                              data-testid="family-name"
                                          >
                                              <span className="grid__family-index">{pad2 (row + 1)}</span>
                                              <span className="grid__family-text">{family.name}</span>
                                              <button
                                                  className="grid__family-colour"
                                                  type="button"
                                                  title={`Recolour ${family.name}`}
                                                  aria-label={`Recolour ${family.name}`}
                                                  data-testid="family-colour"
                                                  data-family={family.id}
                                                  data-colour-index={colour}
                                                  style={{ background: colourForIndex (colour) }}
                                                  onClick={onFamilyColourChange === undefined || slots.length === 0
                                                      ? undefined
                                                      : () => {
                                                          onFamilyColourChange (
                                                              family.id,
                                                              slots,
                                                              nextColourIndex (colour));
                                                      }}
                                              />
                                              <span className="grid__family-count">
                                                  {family.patches.length}/{GRID_COLUMNS}
                                              </span>
                                          </div>

                                          {Array.from ({ length: GRID_COLUMNS }, (_, column) => {
                                              const patch = family.patches[column];

                                              if (patch === undefined)
                                              {
                                                  return (
                                                      <div
                                                          className="grid__cell grid__cell--empty"
                                                          role="gridcell"
                                                          aria-disabled="true"
                                                          aria-label={`${family.name}, empty patch ${String (column + 1)}`}
                                                          key={`empty-${family.id}-${String (column)}`}
                                                          data-testid="grid-cell"
                                                          data-empty="true"
                                                          data-family={family.id}
                                                          data-column={column}
                                                          data-colour-index={0}
                                                          data-colour={colourForIndex (0)}
                                                          data-fill={colourForIndex (0)}
                                                          style={{ background: `linear-gradient(150deg, ${colourForIndex (0)}33 0%, #14151c 70%)` }}
                                                      />
                                                  );
                                              }

                                              const slot = patch.slot;
                                              const armed = slot !== null && slot === model.armedSlot;
                                              const peak = slot === null
                                                  ? 0
                                                  : (meters.slots[slot]?.peak ?? 0);
                                              const latencyMs = patch.latencyMs;
                                              const latencySamples = patch.latencySamples;
                                              const colour = colourForIndex (patch.colorIndex);
                                              const fill = armed ? ARMED_COLOUR : colour;

                                              return (
                                                  <div
                                                      className={`grid__cell${armed ? " grid__cell--armed" : ""}`}
                                                      role="gridcell"
                                                      key={patch.id}
                                                      data-testid="grid-cell"
                                                      data-slot={slot ?? -1}
                                                      data-armed={armed ? "true" : "false"}
                                                      data-empty="false"
                                                      data-family={family.id}
                                                      data-column={column}
                                                      data-colour-index={patch.colorIndex}
                                                      data-colour={colour}
                                                      data-fill={fill}
                                                      data-warm-state={patch.warmState}
                                                      style={{ background: armed
                                                          ? ARMED_COLOUR
                                                          : `linear-gradient(150deg, ${colour}55 0%, #1c1d28 70%)` }}
                                                      aria-label={`${family.name}, ${patch.name}${armed ? ", armed" : ""}`}
                                                  >
                                                      <button
                                                          className="grid__cell-main"
                                                          type="button"
                                                          data-testid="grid-cell-arm"
                                                          aria-pressed={armed}
                                                          onClick={onArm === undefined || slot === null
                                                              ? undefined
                                                              : () => { onArm (slot); }}
                                                      >
                                                          <span className="grid__cell-name">{patch.name}</span>

                                                          <span className="grid__cell-foot">
                                                              <span className="grid__cell-slot">
                                                                  {slot === null ? "—" : `S${pad2 (slot + 1)}`}
                                                              </span>
                                                              {armed
                                                                  ? <span className="grid__cell-badge">Armed</span>
                                                                  : null}
                                                          </span>

                                                          {latencyMs !== undefined
                                                              ? <span
                                                                    className="grid__cell-latency"
                                                                    data-testid="slot-latency"
                                                                    data-slot={slot ?? -1}
                                                                    data-latency-samples={latencySamples ?? 0}
                                                                    data-latency-ms={latencyMs.toFixed (3)}
                                                                    title="Instrument + FX chain latency"
                                                                >
                                                                    {formatLatency (latencyMs)}
                                                                </span>
                                                              : null}

                                                          <span
                                                              className="grid__cell-meter"
                                                              data-testid="slot-meter"
                                                              data-slot={slot ?? -1}
                                                              data-peak={peak.toFixed (3)}
                                                              aria-hidden="true"
                                                          >
                                                              <span
                                                                  className="grid__cell-meter-fill"
                                                                  style={{ width: `${String (meterPercent (peak))}%` }}
                                                              />
                                                          </span>
                                                      </button>

                                                      <button
                                                          className="grid__cell-colour"
                                                          type="button"
                                                          title={`Colour ${patch.colorIndex + 1} for ${patch.name}`}
                                                          aria-label={`Change colour for ${patch.name}`}
                                                          data-testid="cell-colour"
                                                          data-slot={slot ?? -1}
                                                          data-colour-index={patch.colorIndex}
                                                          style={{ background: colour }}
                                                          onClick={onColourChange === undefined || slot === null
                                                              ? undefined
                                                              : () => {
                                                                  onColourChange (slot, nextColourIndex (patch.colorIndex));
                                                              }}
                                                      />
                                                  </div>
                                              );
                                          })}
                                      </div>
                                  );
                              })}
                          </div>
                      </div>}
        </section>
    );
}
