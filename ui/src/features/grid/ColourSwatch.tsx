import type { ReactElement } from "react";

import { colourForIndex, nextColourIndex } from "./colours";

export interface FamilyColourSwatchProps
{
    readonly familyId: string;
    readonly familyName: string;
    readonly colorIndex: number;
    readonly onCycle: ((nextColorIndex: number) => void) | undefined;
}

export function FamilyColourSwatch ({
    familyId,
    familyName,
    colorIndex,
    onCycle,
}: FamilyColourSwatchProps): ReactElement
{
    return (
        <button
            className="grid__family-colour"
            type="button"
            title={`Recolour ${familyName}`}
            aria-label={`Recolour ${familyName}`}
            data-testid="family-colour"
            data-family={familyId}
            data-colour-index={colorIndex}
            style={{ background: colourForIndex (colorIndex) }}
            onClick={onCycle === undefined
                ? undefined
                : () => { onCycle (nextColourIndex (colorIndex)); }}
        />
    );
}

export interface PatchColourSwatchProps
{
    readonly slot: number | null;
    readonly patchName: string;
    readonly colorIndex: number;
    readonly onCycle: ((nextColorIndex: number) => void) | undefined;
}

export function PatchColourSwatch ({
    slot,
    patchName,
    colorIndex,
    onCycle,
}: PatchColourSwatchProps): ReactElement
{
    return (
        <button
            className="grid__cell-colour"
            type="button"
            title={`Colour ${colorIndex + 1} for ${patchName}`}
            aria-label={`Change colour for ${patchName}`}
            data-testid="cell-colour"
            data-slot={slot ?? -1}
            data-colour-index={colorIndex}
            style={{ background: colourForIndex (colorIndex) }}
            onClick={onCycle === undefined
                ? undefined
                : () => { onCycle (nextColourIndex (colorIndex)); }}
        />
    );
}
