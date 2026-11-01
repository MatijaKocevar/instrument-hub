import type { ReactElement } from "react";

import { colourForIndex, nextColourIndex } from "./colours";

export interface FamilyColourSwatchProps
{
    readonly familyId: string;
    readonly familyName: string;
    readonly colorIndex: number;
    /** Recolour the row; omitted when the family has no playable patches. */
    readonly onCycle: ((nextColorIndex: number) => void) | undefined;
}

/** The row swatch that recolours every patch in a family. */
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
    /** Recolour the patch; omitted when the patch has no engine slot. */
    readonly onCycle: ((nextColorIndex: number) => void) | undefined;
}

/** The corner swatch that cycles one patch's palette without arming it. */
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
