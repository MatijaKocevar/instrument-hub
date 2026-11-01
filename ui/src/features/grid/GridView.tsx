import type { ReactElement } from "react";

import { GridHeader } from "./GridHeader";
import { GridTable } from "./GridTable";
import { LoadingGrid } from "./LoadingGrid";
import type { GridModel } from "./model";
import { EMPTY_METERS, type MeterSnapshot } from "./meterFrame";
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
            <GridHeader meters={meters} />

            {loading
                ? <LoadingGrid />
                : model.families.length === 0
                    ? <div className="grid__empty" data-testid="grid-empty">
                          No instruments in this set yet.
                      </div>
                    : <GridTable
                          model={model}
                          meters={meters}
                          onArm={onArm}
                          onColourChange={onColourChange}
                          onFamilyColourChange={onFamilyColourChange}
                      />}
        </section>
    );
}
