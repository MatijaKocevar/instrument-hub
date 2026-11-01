import type { ReactElement } from "react";

import type { ControlState } from "../../api/client";

function formatUptime (milliseconds: number): string
{
    const totalSeconds = Math.max (0, Math.floor (milliseconds / 1000));
    const minutes = Math.floor (totalSeconds / 60);
    const seconds = totalSeconds % 60;

    if (minutes >= 60)
    {
        const hours = Math.floor (minutes / 60);

        return `${hours} h ${minutes % 60} min`;
    }

    return minutes > 0 ? `${minutes} min ${seconds} s` : `${seconds} s`;
}

function slotLabel (state: ControlState): string
{
    if (state.armedSlot < 0)
        return "None";

    const slot = state.slots.find (candidate => candidate.index === state.armedSlot);

    return slot === undefined
        ? `Slot ${state.armedSlot + 1}`
        : `${(state.armedSlot + 1).toString().padStart (2, "0")} · ${slot.name}`;
}

export function EnginePanel ({ state }: { readonly state: ControlState }): ReactElement
{
    return (
        <article className="panel">
            <h2 className="panel__title">Engine</h2>
            <dl className="panel__meta">
                <div>
                    <dt>Application</dt>
                    <dd data-testid="application-name">{state.application.name}</dd>
                </div>
                <div>
                    <dt>Version</dt>
                    <dd data-testid="application-version">{state.application.version}</dd>
                </div>
                <div>
                    <dt>Uptime</dt>
                    <dd>{formatUptime (state.uptimeMs)}</dd>
                </div>
                <div>
                    <dt>Armed slot</dt>
                    <dd data-testid="armed-slot">{slotLabel (state)}</dd>
                </div>
            </dl>
        </article>
    );
}
