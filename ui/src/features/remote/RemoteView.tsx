import type { ReactElement } from "react";

import {
    formatBpm,
    formatPosition,
    type RemoteConnection,
    type RemoteModel,
} from "./model";
import { remoteBanner, type RemoteLinkSnapshot } from "./link";
import { NowPlayingCard } from "./NowPlayingCard";
import { RemoteHeader } from "./RemoteHeader";
import { RemoteLayout } from "./RemoteLayout";
import { SetList } from "./SetList";
import type { RemoteContrast } from "./theme";
import type { WakeLockStatus } from "./wakeLock";
import "./remote.css";

export type { RemoteConnection } from "./model";

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

    The stage header, now-playing card, set list and performance grid live in
    their own components; this file keeps the page shell, banner and transport
    wiring.
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
    const playing = model.transport.playing;
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
            <RemoteHeader
                applicationName={model.applicationName}
                connection={connection}
                contrast={contrast}
                wakeLock={wakeLock}
                onToggleContrast={onToggleContrast}
            />

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

            <RemoteLayout
                now={
                    <NowPlayingCard
                        model={model}
                        loading={loading}
                        onPrevious={onPrevious}
                        onNext={onNext}
                    />
                }
                setList={
                    <SetList
                        model={model}
                        loading={loading}
                        onSelect={onSelect}
                    />
                }
                transport={
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
                }
                panic={
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
                }
            />
        </main>
    );
}
