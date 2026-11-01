import type { ReactElement } from "react";

import { colourForIndex } from "./colours";
import { pad2 } from "./format";
import { FamilyColourSwatch } from "./ColourSwatch";
import { GridCell } from "./GridCell";
import { GRID_COLUMNS, type GridFamily } from "./model";
import type { MeterSnapshot } from "./meterFrame";

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

export interface FamilyRowProps
{
    readonly family: GridFamily;
    readonly row: number;
    readonly armedSlot: number;
    readonly meters: MeterSnapshot;
    readonly onArm: ((slot: number) => void) | undefined;
    readonly onColourChange: ((slot: number, colorIndex: number) => void) | undefined;
    readonly onFamilyColourChange:
        ((familyId: string, slots: readonly number[], colorIndex: number) => void) | undefined;
}

export function FamilyRow ({
    family,
    row,
    armedSlot,
    meters,
    onArm,
    onColourChange,
    onFamilyColourChange,
}: FamilyRowProps): ReactElement
{
    const colour = familyColour (family);
    const slots = familySlots (family);

    return (
        <div
            className="grid__row"
            role="row"
            data-testid="family"
            data-family={family.id}
            data-row={row}
            data-colour-index={colour}
            data-colour={colourForIndex (colour)}
        >
            <div className="grid__family" role="rowheader" data-testid="family-name">
                <span className="grid__family-index">{pad2 (row + 1)}</span>
                <span className="grid__family-text">{family.name}</span>
                <FamilyColourSwatch
                    familyId={family.id}
                    familyName={family.name}
                    colorIndex={colour}
                    onCycle={onFamilyColourChange === undefined || slots.length === 0
                        ? undefined
                        : nextColorIndex => {
                            onFamilyColourChange (family.id, slots, nextColorIndex);
                        }}
                />
                <span className="grid__family-count">
                    {family.patches.length}/{GRID_COLUMNS}
                </span>
            </div>

            {Array.from ({ length: GRID_COLUMNS }, (_, column) => {
                const patch = family.patches[column];

                return (
                    <GridCell
                        key={patch === undefined ? `empty-${family.id}-${String (column)}` : patch.id}
                        familyId={family.id}
                        familyName={family.name}
                        column={column}
                        patch={patch}
                        armedSlot={armedSlot}
                        meters={meters}
                        onArm={onArm}
                        onColourChange={onColourChange}
                    />
                );
            })}
        </div>
    );
}
