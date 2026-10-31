import { useEffect, useMemo, type ReactElement } from "react";

import type { ControlState } from "./api/client";
import { AudioSettingsPanel } from "./features/audio-settings";
import {
    GridView,
    gridFromState,
    metersUrlFor,
    useMeters,
} from "./features/grid";
import {
    followsArmedSlot,
    instrumentSlotForRoute,
    InstrumentPage,
    patchForInstrumentSlot,
} from "./features/instrument";
import { MidiPanel } from "./features/midi";
import { PairingPanel } from "./features/pairing";
import { PluginsPage, pluginAssignmentsFromState } from "./features/plugins";
import { useAppState, useAppStore } from "./state/AppStoreContext";
import {
    instrumentPathForSlot,
    pathForRoute,
    slotFromSearch,
    useRouter,
    type ShellRoute,
} from "./state/router";
import type { ConnectionPhase } from "./state/store";

const PHASE_LABEL: Record<ConnectionPhase, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Connected",
    offline: "Offline",
};

const NAV_ITEMS: readonly { route: ShellRoute; label: string }[] = [
    { route: "grid", label: "Grid" },
    { route: "instrument", label: "Instrument" },
    { route: "plugins", label: "Plugins" },
    { route: "audio", label: "Audio" },
];

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

/**
    Arm a patch and open its Instrument page.

    Exported as a pure function so the click contract (one action both arms and
    routes) is testable without a DOM.
*/
export function armAndOpenInstrument (
    slot: number,
    arm: (slot: number) => void,
    navigate: (path: string) => void): void
{
    arm (slot);
    navigate (instrumentPathForSlot (slot));
}

function SystemPanels ({ state }: { readonly state: ControlState }): ReactElement
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

interface AppProps
{
    /** Render a specific route in the headless snapshot test. */
    readonly initialPath?: string;
}

export default function App ({ initialPath }: AppProps = {}): ReactElement
{
    const app = useAppState();
    const store = useAppStore();
    const router = useRouter (initialPath);
    const state = app.state;
    const grid = useMemo (() => gridFromState (state), [state]);
    const pluginAssignments = useMemo (() => pluginAssignmentsFromState (state), [state]);
    const meters = useMeters ({ url: metersUrlFor (store.baseUrl) });

    const armedSlot = state?.armedSlot ?? -1;
    const requestedSlot = slotFromSearch (router.search);
    const instrumentSlot = instrumentSlotForRoute (router.route, requestedSlot, armedSlot);
    const instrumentPatch = patchForInstrumentSlot (
        state?.families.flatMap (family => family.patches) ?? [],
        instrumentSlot);

    // The insert chain belongs to the Instrument page, and the inventory feeds
    // both pages. Fetch each on arrival; the store de-duplicates the chain per
    // slot and keeps a successful inventory until a rescan replaces it.
    useEffect (() =>
    {
        if (router.route !== "instrument" && router.route !== "plugins")
            return;

        if (router.route === "instrument" && instrumentSlot !== null)
            void store.loadFxChain (instrumentSlot);

        if (app.plugins === null && ! app.pluginBusy)
            void store.loadPlugins ();
    }, [router.route, instrumentSlot, store, app.plugins, app.pluginBusy]);

    // The audio page also shows the MIDI inputs. The normal poll carries them,
    // but a direct route render (and the snapshot test) may arrive first.
    useEffect (() =>
    {
        if (router.route !== "audio" || app.midi !== null || app.midiBusy)
            return;

        void store.loadMidi ();
    }, [router.route, store, app.midi, app.midiBusy]);

    const openPatch = (slot: number): void =>
    {
        armAndOpenInstrument (
            slot,
            candidate => { void store.armSlot (candidate); },
            router.navigate);
    };

    const changeColour = (slot: number, colorIndex: number): void =>
    {
        void store.updatePatch (slot, { colorIndex });
    };

    return (
        <main className="app" data-route={router.route}>
            <header className="app__header">
                <p className="app__eyebrow">Audio workspace</p>
                <h1 className="app__title" data-testid="app-title">
                    Instrument Hub
                </h1>
                <p className="app__lede">
                    One place for your instruments, controller and set.
                </p>

                <nav className="app__nav" aria-label="Pages" data-testid="app-nav">
                    {NAV_ITEMS.map (item => (
                        <a
                            className={`app__nav-link${router.route === item.route ? " app__nav-link--active" : ""}`}
                            href={pathForRoute (item.route)}
                            key={item.route}
                            data-testid={`nav-${item.route}`}
                            data-active={router.route === item.route ? "true" : "false"}
                            aria-current={router.route === item.route ? "page" : undefined}
                            onClick={event =>
                            {
                                event.preventDefault ();
                                router.navigate (pathForRoute (item.route));
                            }}
                        >
                            {item.label}
                        </a>
                    ))}
                </nav>
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
                    onClick={() => { void store.refresh (); }}
                >
                    Retry now
                </button>
            </section>

            {state === null && router.route !== "grid"
                ? <section
                      className="app__empty"
                      aria-label="Engine state"
                      data-testid="state-placeholder"
                  >
                      Waiting for the engine to report its state…
                  </section>
                : null}

            <div className="app__route">
                {router.route === "grid"
                    ? <>
                          {state === null
                              ? null
                              : <SystemPanels state={state} />}

                          <GridView
                              model={grid}
                              meters={meters}
                              loading={state === null}
                              onArm={openPatch}
                              onColourChange={changeColour}
                              onFamilyColourChange={(familyId, _slots, colorIndex) => {
                                  void store.updateFamilyColour (familyId, colorIndex);
                              }}
                          />
                      </>
                    : router.route === "instrument"
                        ? <InstrumentPage
                              patch={instrumentPatch}
                              armedSlot={armedSlot}
                              following={followsArmedSlot (armedSlot)}
                              chain={app.fxSlot === instrumentSlot ? app.fxChain : null}
                              chainError={app.fxSlot === instrumentSlot ? app.fxError : null}
                              chainBusy={app.fxBusy}
                              sourceBusy={app.sourceBusy}
                              plugins={app.plugins?.plugins ?? null}
                              pluginError={app.pluginError}
                              loading={state === null}
                              onBack={() => { router.navigate (pathForRoute ("grid")); }}
                              onColourChange={changeColour}
                              onVolumeChange={(slot, volumeDb) => {
                                  void store.updatePatch (slot, { volumeDb });
                              }}
                              onPanChange={(slot, pan) => {
                                  void store.updatePatch (slot, { pan });
                              }}
                              onMuteChange={(slot, mute) => {
                                  void store.updatePatch (slot, { mute });
                              }}
                              onSoloChange={(slot, solo) => {
                                  void store.updatePatch (slot, { solo });
                              }}
                              onSourceChange={(slot, source) => {
                                  void store.changeInstrumentSource (slot, source);
                              }}
                              onAddInsert={(slot, insert) => {
                                  void store.addFxInsert (slot, insert);
                              }}
                              onRemoveInsert={(slot, index) => {
                                  void store.removeFxInsert (slot, index);
                              }}
                              onToggleBypass={(slot, index, bypassed) => {
                                  void store.updateFxInsert (slot, index, { bypassed });
                              }}
                              onMoveInsert={(slot, from, to) => {
                                  void store.moveFxInsert (slot, from, to);
                              }}
                              onRetryChain={slot => { void store.loadFxChain (slot, { force: true }); }}
                          />
                        : router.route === "plugins"
                            ? <PluginsPage
                                  inventory={app.plugins}
                                  assignments={pluginAssignments}
                                  error={app.pluginError}
                                  busy={app.pluginBusy}
                                  onRescan={() => { void store.rescanPlugins (); }}
                                  onReload={() => { void store.loadPlugins (); }}
                              />
                            : <section className="app__stack" data-testid="audio-page">
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

                                  <MidiPanel
                                      midi={app.midi}
                                      error={app.midiError}
                                      busy={app.midiBusy}
                                      padTestBusy={app.padTestBusy}
                                      padTestError={app.padTestError}
                                      loading={app.midi === null}
                                      onToggleInput={(id, enabled) => {
                                          void store.toggleMidiInput (id, enabled);
                                      }}
                                      onPadTest={() => { void store.runPadTest (); }}
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
                              </section>}
            </div>

            <footer className="app__footer">
                <span className="app__status" data-testid="app-status">
                    {state === null
                        ? "Waiting for device state"
                        : `Connected · ${state.slots.length} slots · armed ${armedSlot < 0 ? "none" : armedSlot + 1}`}
                </span>
                <span className="app__version">v0.1.0</span>
            </footer>
        </main>
    );
}
