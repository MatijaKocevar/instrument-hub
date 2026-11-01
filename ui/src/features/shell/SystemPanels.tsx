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

function formatBpm (bpm: number): string
{
    return `${Number.isInteger (bpm) ? bpm.toFixed (0) : bpm.toFixed (1)} BPM`;
}

function formatGain (gain: number): string
{
    return `${Math.round (gain * 100)}%`;
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

/** Engine, transport and settings summaries shown above the Grid. */
export function SystemPanels ({ state }: { readonly state: ControlState }): ReactElement
{
    return (
        <section className="app__panels" aria-label="Engine state">
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

            <article className="panel">
                <h2 className="panel__title">Transport</h2>
                <dl className="panel__meta">
                    <div>
                        <dt>State</dt>
                        <dd data-testid="transport-state">
                            {state.transport.playing ? "Playing" : "Stopped"}
                        </dd>
                    </div>
                    <div>
                        <dt>Tempo</dt>
                        <dd data-testid="transport-bpm">{formatBpm (state.transport.bpm)}</dd>
                    </div>
                    <div>
                        <dt>Position</dt>
                        <dd>{state.transport.ppq.toFixed (2)} beats</dd>
                    </div>
                    <div>
                        <dt>Samples</dt>
                        <dd>{state.transport.positionSamples.toLocaleString ("en-US")}</dd>
                    </div>
                </dl>
            </article>

            <article className="panel">
                <h2 className="panel__title">Settings</h2>
                <dl className="panel__meta">
                    <div>
                        <dt>Master gain</dt>
                        <dd>{formatGain (state.settings.masterGain)}</dd>
                    </div>
                    <div>
                        <dt>Input gain</dt>
                        <dd>{formatGain (state.settings.inputGain)}</dd>
                    </div>
                    <div>
                        <dt>MIDI channel</dt>
                        <dd>{state.settings.midiChannel}</dd>
                    </div>
                    <div>
                        <dt>Metronome</dt>
                        <dd>{state.settings.metronome ? "On" : "Off"}</dd>
                    </div>
                </dl>
            </article>
        </section>
    );
}
