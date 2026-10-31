import type { ReactElement } from "react";

import {
    currentPatch,
    formatBpm,
    formatPosition,
    type RemoteConnection,
    type RemoteModel,
} from "./model";
import { remoteBanner, type RemoteLinkSnapshot } from "./link";
import { contrastLabel, type RemoteContrast } from "./theme";
import type { WakeLockStatus } from "./wakeLock";
import "./remote.css";

export type { RemoteConnection } from "./model";

const CONNECTION_LABEL: Record<RemoteConnection, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Live",
    offline: "Offline",
};

export interface RemoteViewProps
{
    /** Set list, armed patch and transport from `GET /state`. */
    readonly model: RemoteModel;
    /** Connection life-cycle, shown in the header. */
    readonly connection?: RemoteConnection;
    /** Show the loading state until the first state snapshot arrives. */
    readonly loading?: boolean;
    /** Last API failure, shown under the header. */
    readonly error?: string | null;
    /** Live WebSocket history, drives the reconnect banner. */
    readonly link?: RemoteLinkSnapshot | null;
    /** Stage palette; high contrast is the default. */
    readonly contrast?: RemoteContrast;
    /** Screen-wake-lock status, shown as a small badge. */
    readonly wakeLock?: WakeLockStatus;
    /** Switch between the high-contrast and standard palettes. */
    readonly onToggleContrast?: () => void;
    /** Tap a patch in the set list. */
    readonly onSelect?: (slot: number) => void;
    /** Move one patch backwards in set order. */
    readonly onPrevious?: () => void;
    /** Move one patch forwards in set order. */
    readonly onNext?: () => void;
    /** Play/pause toggle. */
    readonly onTogglePlay?: () => void;
    /** Stop and rewind the transport. */
    readonly onStop?: () => void;
    /** Stop the transport and disarm every patch. */
    readonly onPanic?: () => void;
}

function pad2 (value: number): string
{
    return value.toString ().padStart (2, "0");
}

function formatLatency (milliseconds: number): string
{
    const clamped = Math.max (0, Number.isFinite (milliseconds) ? milliseconds : 0);

    return `${clamped.toFixed (1)} ms`;
}

/** `live` / `lost` / `connecting` / `unknown`, for data attributes. */
function linkPhase (link: RemoteLinkSnapshot | null): string
{
    if (link === null)
        return "unknown";

    if (link.connected)
        return "live";

    return link.everConnected ? "lost" : "connecting";
}

/**
    The tablet/stage remote.

    Everything is sized for a finger: a 44 px floor on every control, two
    enormous patch-step buttons, a scrollable set list and a panic button that
    cannot be missed. The layout is a single column below 800 px and a
    two-column performance layout above it.
*/
export function RemoteView ({
    model,
    connection = "idle",
    loading = false,
    error = null,
    link = null,
    contrast = "high",
    wakeLock = "idle",
    onToggleContrast,
    onSelect,
    onPrevious,
    onNext,
    onTogglePlay,
    onStop,
    onPanic,
}: RemoteViewProps): ReactElement
{
    const patch = currentPatch (model);
    const playing = model.transport.playing;
    const empty = model.patches.length === 0;
    const stepDisabled = empty || loading;
    const banner = remoteBanner (connection, link);

    return (
        <main
            className="remote"
            aria-label="Stage remote"
            data-testid="remote"
            data-theme="stage"
            data-contrast={contrast}
            data-connection={connection}
            data-link={linkPhase (link)}
            data-wake-lock={wakeLock}
            data-armed-slot={model.armedSlot}
            data-playing={playing ? "true" : "false"}
        >
            <header className="remote__header">
                <div className="remote__heading">
                    <p className="remote__eyebrow">Stage remote</p>
                    <h1 className="remote__title">{model.applicationName}</h1>
                </div>

                <div className="remote__header-actions">
                    {wakeLock === "active"
                        ? <span
                              className="remote__wake"
                              data-testid="remote-wake-lock"
                              data-state="active"
                          >
                              Screen on
                          </span>
                        : wakeLock === "denied" || wakeLock === "unavailable"
                            ? <span
                                  className="remote__wake remote__wake--off"
                                  data-testid="remote-wake-lock"
                                  data-state={wakeLock}
                              >
                                  Screen may sleep
                              </span>
                            : null}

                    <button
                        className="remote__contrast"
                        type="button"
                        data-testid="remote-contrast"
                        data-contrast={contrast}
                        aria-pressed={contrast === "high"}
                        aria-label="Toggle the high-contrast stage theme"
                        onClick={onToggleContrast}
                    >
                        {contrastLabel (contrast)}
                    </button>

                    <span
                        className={`remote__status remote__status--${connection}`}
                        data-testid="remote-connection"
                        data-phase={connection}
                    >
                        {CONNECTION_LABEL[connection]}
                    </span>
                </div>
            </header>

            {banner.visible
                ? <div
                      className={`remote__banner remote__banner--${banner.tone}`}
                      role={banner.tone === "critical" ? "alert" : "status"}
                      data-testid="remote-banner"
                      data-tone={banner.tone}
                      data-link={linkPhase (link)}
                      data-failures={link?.failures ?? 0}
                  >
                      <span className="remote__banner-dot" aria-hidden="true" />
                      <span className="remote__banner-text">
                          <strong className="remote__banner-title">{banner.title}</strong>
                          <span className="remote__banner-detail">{banner.detail}</span>
                      </span>
                  </div>
                : null}

            {error !== null
                ? <p className="remote__error" role="alert" data-testid="remote-error">
                      {error}
                  </p>
                : null}

            <div className="remote__layout">
                <section
                    className="remote__now"
                    aria-label="Armed patch"
                    aria-live="polite"
                    data-testid="remote-current"
                    data-slot={patch?.slot ?? -1}
                >
                    <div className="remote__now-text">
                        <p className="remote__label">Now playing</p>
                        <h2 className="remote__now-name" data-testid="remote-current-name">
                            {loading
                                ? "Loading set…"
                                : patch === null
                                    ? "Nothing armed"
                                    : patch.name}
                        </h2>
                        <p className="remote__now-meta">
                            {patch === null
                                ? "Tap a patch or use Next to begin"
                                : `Patch ${pad2 (model.armedSlot + 1)} · chain ${formatLatency (patch.latencyMs)}`}
                        </p>
                    </div>

                    <div className="remote__step">
                        <button
                            className="remote__step-button"
                            type="button"
                            data-testid="remote-prev"
                            aria-label="Previous patch"
                            disabled={stepDisabled}
                            onClick={onPrevious}
                        >
                            <span className="remote__step-arrow" aria-hidden="true">‹</span>
                            <span className="remote__step-label">Prev</span>
                        </button>

                        <button
                            className="remote__step-button"
                            type="button"
                            data-testid="remote-next"
                            aria-label="Next patch"
                            disabled={stepDisabled}
                            onClick={onNext}
                        >
                            <span className="remote__step-label">Next</span>
                            <span className="remote__step-arrow" aria-hidden="true">›</span>
                        </button>
                    </div>
                </section>

                <section
                    className="remote__set"
                    aria-label="Set list"
                    data-testid="remote-set"
                >
                    <header className="remote__section-head">
                        <h2 className="remote__section-title">Set list</h2>
                        <span className="remote__count" data-testid="remote-set-count">
                            {model.patches.length} {model.patches.length === 1 ? "patch" : "patches"}
                        </span>
                    </header>

                    {loading
                        ? <div className="remote__set-loading" role="status" aria-busy="true">
                              Loading the set…
                          </div>
                        : empty
                            ? <div className="remote__empty" data-testid="remote-empty">
                                  No patches in this set yet.
                              </div>
                            : <ol className="remote__set-list" data-testid="remote-set-list">
                                  {model.patches.map ((candidate, index) => {
                                      const armed = candidate.slot === model.armedSlot;

                                      return (
                                          <li className="remote__set-row" key={candidate.id}>
                                              <button
                                                  className={`remote__set-item${armed ? " remote__set-item--armed" : ""}`}
                                                  type="button"
                                                  data-testid="remote-set-item"
                                                  data-slot={candidate.slot}
                                                  data-armed={armed ? "true" : "false"}
                                                  aria-current={armed ? "true" : undefined}
                                                  aria-pressed={armed}
                                                  onClick={onSelect === undefined
                                                      ? undefined
                                                      : () => { onSelect (candidate.slot); }}
                                              >
                                                  <span className="remote__set-index">
                                                      {pad2 (index + 1)}
                                                  </span>
                                                  <span className="remote__set-name">
                                                      {candidate.name}
                                                  </span>
                                                  <span className="remote__set-latency">
                                                      {formatLatency (candidate.latencyMs)}
                                                  </span>
                                                  {armed
                                                      ? <span className="remote__set-badge">Now</span>
                                                      : null}
                                              </button>
                                          </li>
                                      );
                                  })}
                              </ol>}
                </section>

                <section
                    className="remote__transport"
                    aria-label="Transport"
                    data-testid="remote-transport"
                    data-playing={playing ? "true" : "false"}
                >
                    <header className="remote__section-head">
                        <h2 className="remote__section-title">Transport</h2>
                        <span
                            className={`remote__transport-state${playing ? " remote__transport-state--playing" : ""}`}
                            data-testid="remote-transport-state"
                        >
                            {playing ? "Playing" : "Stopped"}
                        </span>
                    </header>

                    <p className="remote__bpm" data-testid="remote-bpm">
                        {formatBpm (model.transport.bpm)}
                    </p>
                    <p className="remote__position" data-testid="remote-position">
                        Bar {formatPosition (model.transport.ppq)}
                        <span className="remote__position-detail">
                            {model.transport.positionSamples.toLocaleString ("en-US")} samples
                        </span>
                    </p>

                    <div className="remote__transport-actions">
                        <button
                            className="remote__transport-button remote__transport-button--play"
                            type="button"
                            data-testid="remote-play"
                            aria-pressed={playing}
                            onClick={onTogglePlay}
                        >
                            {playing ? "Pause" : "Play"}
                        </button>
                        <button
                            className="remote__transport-button"
                            type="button"
                            data-testid="remote-stop"
                            onClick={onStop}
                        >
                            Stop
                        </button>
                    </div>
                </section>

                <section
                    className="remote__panic"
                    aria-label="Panic"
                    data-testid="remote-panic-panel"
                >
                    <button
                        className="remote__panic-button"
                        type="button"
                        data-testid="remote-panic"
                        onClick={onPanic}
                    >
                        Panic
                    </button>
                    <p className="remote__panic-hint">
                        Stops the transport and disarms every patch.
                    </p>
                </section>
            </div>
        </main>
    );
}
