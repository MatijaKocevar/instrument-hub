import type { ReactElement } from "react";

import type { ControlPatch } from "../../api/client";
import { colourForIndex, nextColourIndex } from "../grid";
import { latencyReadout } from "./latencyReadout";

interface InstrumentHeadProps
{
    readonly patch: ControlPatch;
    readonly armed: boolean;
    readonly following: boolean;
    readonly onColourChange?: (slot: number, colorIndex: number) => void;
}

/** Instrument page header: identity, arm state and the live patch badges. */
export function InstrumentHead ({ patch, armed, following, onColourChange }: InstrumentHeadProps): ReactElement
{
    const colour = colourForIndex (patch.colorIndex);

    return (
        <header className="instrument__head">
            <p className="instrument__eyebrow">
                Instrument · {patch.familyName}
            </p>
            <h2 className="instrument__title" data-testid="instrument-name">
                {patch.name}
            </h2>
            <p className="instrument__lede">
                {patch.familyName} · slot {String (patch.slot + 1).padStart (2, "0")}
                {armed ? " · armed" : " · not armed"}
                {following ? " · following the controller" : " · pinned selection"}
            </p>

            <div className="instrument__badges">
                <span
                    className="instrument__badge instrument__badge--warm"
                    data-testid="instrument-warm-state"
                    data-warm={patch.warm ? "true" : "false"}
                    data-warm-state={patch.warmState}
                >
                    {patch.warmState}
                </span>
                <span
                    className="instrument__badge instrument__badge--latency"
                    data-testid="instrument-latency"
                    data-latency-samples={patch.latencySamples}
                    data-latency-ms={patch.latencyMs.toFixed (3)}
                >
                    {latencyReadout (patch.latencySamples, patch.latencyMs)}
                </span>
                <span
                    className="instrument__badge instrument__badge--colour"
                    data-testid="instrument-colour"
                    data-colour-index={patch.colorIndex}
                    data-colour={colour}
                    style={{ background: colour }}
                >
                    colour {String (patch.colorIndex + 1)}
                </span>
                <button
                    className="instrument__button instrument__button--small"
                    type="button"
                    data-testid="instrument-colour-next"
                    onClick={() => { onColourChange?.(patch.slot, nextColourIndex (patch.colorIndex)); }}
                >
                    Next colour
                </button>
            </div>
        </header>
    );
}
