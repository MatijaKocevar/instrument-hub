import { useEffect, useMemo, useState, type ReactElement } from "react";

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
import { PluginsPage, pluginAssignmentsFromState } from "./features/plugins";
import { RemoteRoute } from "./features/remote";
import { SettingsPage } from "./features/settings";
import { useAppState, useAppStore } from "./state/AppStoreContext";
import {
    instrumentPathForSlot,
    pathForRoute,
    slotFromSearch,
    useRouter,
    type RouterState,
    type ShellRoute,
} from "./state/router";
import type { ConnectionPhase } from "./state/store";

const PHASE_LABEL: Record<ConnectionPhase, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Connected",
    offline: "Offline",
};

/** Width at or below which the sidebar collapses to bottom tabs. */
const NARROW_BREAKPOINT = 900;

interface NavItem
{
    readonly route: ShellRoute;
    readonly label: string;
    readonly shortLabel: string;
}

const WORKSPACE_ITEMS: readonly NavItem[] = [
    { route: "grid", label: "Grid", shortLabel: "Grid" },
    { route: "instrument", label: "Instrument", shortLabel: "Instrument" },
    { route: "plugins", label: "Plugins", shortLabel: "Plugins" },
    { route: "audio", label: "Audio & MIDI", shortLabel: "Audio" },
];

const SETTINGS_ITEM: NavItem = {
    route: "settings",
    label: "Settings & Account",
    shortLabel: "Settings",
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

/**
    One icon per workspace route.

    Inline SVG keeps the shell dependency-free and lets the icon inherit the
    active link colour; the text label is the accessible name.
*/
function RouteIcon ({ route }: { readonly route: ShellRoute }): ReactElement
{
    switch (route)
    {
        case "grid":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <rect x="3" y="3" width="7" height="7" rx="1.5" />
                    <rect x="14" y="3" width="7" height="7" rx="1.5" />
                    <rect x="3" y="14" width="7" height="7" rx="1.5" />
                    <rect x="14" y="14" width="7" height="7" rx="1.5" />
                </svg>
            );

        case "instrument":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <path d="M4 6h8M18 6h2M4 12h3M11 12h9M4 18h8M16 18h4" />
                    <circle cx="14" cy="6" r="2" />
                    <circle cx="9" cy="12" r="2" />
                    <circle cx="14" cy="18" r="2" />
                </svg>
            );

        case "plugins":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <rect x="3" y="3" width="8" height="8" rx="1.5" />
                    <rect x="13" y="13" width="8" height="8" rx="1.5" />
                    <path d="M13 7h4a2 2 0 0 1 2 2v2M7 13v4a2 2 0 0 0 2 2h2" />
                </svg>
            );

        case "audio":
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <path d="M3 12h2l2-5 3 10 3-8 2 5 2-2h4" />
                </svg>
            );

        case "settings":
        case "remote":
        default:
            return (
                <svg viewBox="0 0 24 24" width="20" height="20" fill="none"
                     stroke="currentColor" strokeWidth="1.8"
                     strokeLinecap="round" strokeLinejoin="round">
                    <circle cx="12" cy="12" r="3" />
                    <path d="M12 3v2.5M12 18.5V21M3 12h2.5M18.5 12H21M5.6 5.6l1.8 1.8M16.6 16.6l1.8 1.8M18.4 5.6l-1.8 1.8M7.4 16.6l-1.8 1.8" />
                </svg>
            );
    }
}

interface ShellNavProps
{
    readonly route: ShellRoute;
    readonly navigate: (path: string) => void;
    readonly phase: ConnectionPhase;
    readonly lanEnabled: boolean;
    readonly paired: boolean;
    readonly version: string;
}

function NavLink ({ item, route, navigate, compact = false }: {
    readonly item: NavItem;
    readonly route: ShellRoute;
    readonly navigate: (path: string) => void;
    readonly compact?: boolean;
}): ReactElement
{
    const active = route === item.route;
    const className = compact
        ? `shell__tab${active ? " shell__tab--active" : ""}`
        : `shell__link${active ? " shell__link--active" : ""}`;

    return (
        <a
            className={className}
            href={pathForRoute (item.route)}
            data-testid={`nav-${item.route}`}
            data-active={active ? "true" : "false"}
            data-workspace-item={item.route === "settings" ? "false" : "true"}
            aria-current={active ? "page" : undefined}
            title={compact ? item.label : undefined}
            onClick={event =>
            {
                event.preventDefault ();
                navigate (pathForRoute (item.route));
            }}
        >
            <span className="shell__icon" aria-hidden="true">
                <RouteIcon route={item.route} />
            </span>
            <span className="shell__link-label">
                {compact ? item.shortLabel : item.label}
            </span>
        </a>
    );
}

/**
    Persistent desktop sidebar.

    Workspace destinations sit at the top; Settings & Account and the live
    connection state form the bottom group. Content pages never repeat these
    route links.
*/
function Sidebar ({ route, navigate, phase, lanEnabled, paired, version }: ShellNavProps): ReactElement
{
    return (
        <aside className="shell__sidebar" data-testid="app-sidebar" data-collapsed="false">
            <div className="shell__brand">
                <span className="shell__mark" aria-hidden="true">IH</span>
                <span className="shell__brand-text">
                    <span className="shell__brand-name" data-testid="sidebar-title">
                        Instrument Hub
                    </span>
                    <span className="shell__brand-lede">Audio workspace</span>
                </span>
            </div>

            <nav className="shell__nav" aria-label="Workspace" data-testid="app-nav">
                <p className="shell__nav-title">Workspace</p>
                {WORKSPACE_ITEMS.map (item => (
                    <NavLink
                        key={item.route}
                        item={item}
                        route={route}
                        navigate={navigate}
                    />
                ))}
            </nav>

            <div className="shell__account" data-testid="settings-group">
                <p className="shell__nav-title">Settings &amp; Account</p>
                <nav className="shell__nav" aria-label="Settings and account" data-testid="settings-nav">
                    <NavLink item={SETTINGS_ITEM} route={route} navigate={navigate} />
                </nav>

                <p
                    className="shell__session"
                    data-testid="sidebar-session"
                    data-lan={lanEnabled ? "true" : "false"}
                    data-paired={paired ? "true" : "false"}
                >
                    <span className={`shell__dot shell__dot--${phase}`} aria-hidden="true" />
                    {PHASE_LABEL[phase]}
                    <span className="shell__session-detail">
                        {lanEnabled ? "LAN on" : "LAN off"}
                    </span>
                </p>
                <p className="shell__version" data-testid="sidebar-version">v{version}</p>
            </div>
        </aside>
    );
}

/** Narrow/tablet navigation: the collapsed form of the sidebar. */
function BottomTabs ({ route, navigate }: ShellNavProps): ReactElement
{
    return (
        <nav
            className="shell__tabs"
            aria-label="Workspace"
            data-testid="app-bottom-tabs"
            data-collapsed="true"
        >
            {[...WORKSPACE_ITEMS, SETTINGS_ITEM].map (item => (
                <NavLink
                    key={item.route}
                    item={item}
                    route={route}
                    navigate={navigate}
                    compact
                />
            ))}
        </nav>
    );
}

/**
    The viewport width is only needed to choose the shell variant. In the app
    the window is the source; the snapshot test supplies a width because
    `react-dom/server` has no viewport.
*/
function useViewportWidth (initialWidth?: number | undefined): number
{
    const [width, setWidth] = useState<number> (
        () => initialWidth ?? (typeof window === "undefined" ? 1280 : window.innerWidth));

    useEffect (() =>
    {
        if (typeof window === "undefined")
            return undefined;

        const onResize = (): void => { setWidth (window.innerWidth); };

        window.addEventListener ("resize", onResize);

        return () => { window.removeEventListener ("resize", onResize); };
    }, []);

    return width;
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
    /** Viewport width in the headless snapshot test (the browser reads its own). */
    readonly viewportWidth?: number | undefined;
}

interface AppShellProps
{
    readonly router: RouterState;
    readonly viewportWidth?: number | undefined;
}

function AppShell ({ router, viewportWidth }: AppShellProps): ReactElement
{
    const app = useAppState ();
    const store = useAppStore ();
    const state = app.state;
    const grid = useMemo (() => gridFromState (state), [state]);
    const pluginAssignments = useMemo (() => pluginAssignmentsFromState (state), [state]);
    const meters = useMeters ({ url: metersUrlFor (store.baseUrl) });
    const width = useViewportWidth (viewportWidth);
    const narrow = width < NARROW_BREAKPOINT;

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

    const version = state?.application.version ?? "0.1.0";
    const navProps: ShellNavProps = {
        route: router.route,
        navigate: router.navigate,
        phase: app.phase,
        lanEnabled: app.pairing?.lanEnabled === true,
        paired: store.token !== null,
        version,
    };

    return (
        <div
            className={`app-shell app-shell--${narrow ? "narrow" : "wide"}`}
            data-testid="app-shell"
            data-layout={narrow ? "narrow" : "wide"}
            data-viewport-width={width}
        >
            {narrow ? null : <Sidebar {...navProps} />}

            <div className="app-shell__main">
                <main className="app" data-route={router.route}>
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
                                      editorOpen={instrumentPatch?.editorOpen === true}
                                      editorBusy={app.editorBusy}
                                      editorError={app.editorError}
                                      captureBusy={app.captureBusy}
                                      capture={app.captureSlot === instrumentSlot ? app.capture : null}
                                      captureError={app.captureSlot === instrumentSlot ? app.captureError : null}
                                      plugins={app.plugins?.plugins ?? null}
                                      pluginError={app.pluginError}
                                      loading={state === null}
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
                                      onOpenEditor={slot => { void store.openInstrumentEditor (slot); }}
                                      onCloseEditor={slot => { void store.closeInstrumentEditor (slot); }}
                                      onCaptureState={slot => { void store.savePatchSound (slot); }}
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
                                    : router.route === "settings"
                                        ? <SettingsPage
                                              pairing={app.pairing}
                                              token={store.token}
                                              pairingError={app.pairingError}
                                              pairingBusy={app.pairingBusy}
                                              connection={app.phase}
                                              baseUrl={store.baseUrl}
                                              application={state?.application ?? null}
                                              schemaVersion={state?.schemaVersion ?? null}
                                              uptimeMs={state?.uptimeMs ?? null}
                                              onToggleLan={enabled => { void store.toggleLan (enabled); }}
                                              onForget={() => { void store.forgetPairing (); }}
                                              onPair={code => { void store.pair (code); }}
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
                                          </section>}
                    </div>

                    <footer className="app__footer">
                        <span className="app__status" data-testid="app-status">
                            {state === null
                                ? "Waiting for device state"
                                : `Connected · ${state.slots.length} slots · armed ${armedSlot < 0 ? "none" : armedSlot + 1}`}
                        </span>
                        <span className="app__version">v{version}</span>
                    </footer>
                </main>
            </div>

            {narrow ? <BottomTabs {...navProps} /> : null}
        </div>
    );
}

export default function App ({ initialPath, viewportWidth }: AppProps = {}): ReactElement
{
    const router = useRouter (initialPath);

    // `/remote` is the tablet surface: it owns the whole window and never gets
    // the desktop sidebar, even if the shell bundle happens to render it.
    if (router.route === "remote")
        return <RemoteRoute />;

    return <AppShell router={router} viewportWidth={viewportWidth} />;
}
