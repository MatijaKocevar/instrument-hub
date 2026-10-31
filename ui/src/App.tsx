import { useMemo } from "react";

import type { ControlState } from "./api/client";
import { AudioSettingsPanel } from "./features/audio-settings";
import { GridView, gridFromState, metersUrlFor, useMeters } from "./features/grid";
import { PairingPanel } from "./features/pairing";
import { useAppState, useAppStore } from "./state/AppStoreContext";
import type { ConnectionPhase } from "./state/store";

const PHASE_LABEL: Record<ConnectionPhase, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Connected",
    offline: "Offline",
};

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

export default function App ()
{
    const app = useAppState();
    const store = useAppStore();
    const state = app.state;
    const armedSlot = state?.slots.find (slot => slot.armed) ?? null;
    const grid = useMemo (() => gridFromState (state), [state]);
    const meters = useMeters ({ url: metersUrlFor (store.baseUrl) });

    return (
        <main className="app">
            <header className="app__header">
                <p className="app__eyebrow">Audio workspace</p>
                <h1 className="app__title" data-testid="app-title">
                    Instrument Hub
                </h1>
                <p className="app__lede">
                    One place for your instruments, controller and set.
                </p>
            </header>

            <section
                className={`app__connection app__connection--${app.phase}`}
                aria-label="Control API connection"
                data-testid="connection"
                data-phase={app.phase}
            >
                <span className="status-pill" data-testid="connection-status">
                    {PHASE_LABEL[app.phase]}
                </span>

                <span className="app__connection-detail">
                    <code>{store.baseUrl}</code>
                </span>

                {app.failedAttempts > 0
                    ? <span className="app__connection-detail">
                          {app.failedAttempts} failed {app.failedAttempts === 1 ? "attempt" : "attempts"}
                      </span>
                    : null}

                {app.error !== null
                    ? <span className="app__connection-error" data-testid="connection-error">
                          {app.error}
                      </span>
                    : null}

                <button
                    className="app__retry"
                    type="button"
                    onClick={() => { void store.refresh(); }}
                >
                    Retry now
                </button>
            </section>

            {state === null
                ? <section
                      className="app__empty"
                      aria-label="Engine state"
                      data-testid="state-placeholder"
                  >
                      Waiting for the engine to report its state…
                  </section>
                : <section className="app__panels" aria-label="Engine state">
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
                  </section>}

            <AudioSettingsPanel
                audio={app.audio}
                measurement={app.measurement}
                error={app.audioError}
                measurementError={app.measurementError}
                busy={app.audioBusy}
                loading={state === null && app.audio === null}
                onChange={patch => { void store.updateAudio (patch); }}
                onMeasure={() => { void store.measureLatency (); }}
            />

            <PairingPanel
                pairing={app.pairing}
                token={store.token}
                error={app.pairingError}
                busy={app.pairingBusy}
                loading={app.pairing === null}
                onToggleLan={enabled => { void store.toggleLan (enabled); }}
                onForget={() => { void store.forgetPairing (); }}
                onPair={code => { void store.pair (code); }}
            />

            <GridView
                model={grid}
                meters={meters}
                loading={state === null}
                onArm={slot => { void store.armSlot (slot); }}
            />

            <footer className="app__footer">
                <span className="app__status" data-testid="app-status">
                    {state === null
                        ? "Waiting for device state"
                        : `Connected · ${state.slots.length} slots · armed ${armedSlot === null ? "none" : armedSlot.index + 1}`}
                </span>
                <span className="app__version">v0.1.0</span>
            </footer>
        </main>
    );
}
