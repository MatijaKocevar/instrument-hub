import type { ReactElement } from "react";

import { pad2 } from "./format";
import { FamilyRow } from "./FamilyRow";
import { GRID_COLUMNS, type GridModel } from "./model";
import type { MeterSnapshot } from "./meterFrame";

export interface GridTableProps
{
    readonly model: GridModel;
    readonly meters: MeterSnapshot;
    readonly onArm: ((slot: number) => void) | undefined;
    readonly onColourChange: ((slot: number, colorIndex: number) => void) | undefined;
    readonly onFamilyColourChange:
        ((familyId: string, slots: readonly number[], colorIndex: number) => void) | undefined;
}

/** The scrollable 8×8 table: column headers, family rows and their cells. */
export function GridTable ({
    model,
    meters,
    onArm,
    onColourChange,
    onFamilyColourChange,
}: GridTableProps): ReactElement
{
    return (
        <div className="grid__scroll">
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

                {model.families.map ((family, row) => (
                    <FamilyRow
                        key={family.id}
                        family={family}
                        row={row}
                        armedSlot={model.armedSlot}
                        meters={meters}
                        onArm={onArm}
                        onColourChange={onColourChange}
                        onFamilyColourChange={onFamilyColourChange}
                    />
                ))}
            </div>
        </div>
    );
}
