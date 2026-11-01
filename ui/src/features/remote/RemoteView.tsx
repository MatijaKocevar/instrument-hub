import type { ReactElement } from "react";

import { linkPhase, type RemoteLinkSnapshot } from "./MeterLink";
import type { RemoteConnection, RemoteModel } from "./model";
import { NowPlayingCard } from "./NowPlayingCard";
import { ReconnectBanner } from "./ReconnectBanner";
import { RemoteHeader } from "./RemoteHeader";
import { RemoteLayout } from "./RemoteLayout";
import { RemotePanic } from "./RemotePanic";
import { RemoteTransport } from "./RemoteTransport";
import { SetList } from "./SetList";
import type { RemoteContrast } from "./theme";
import type { WakeLockStatus } from "./wakeLockTypes";
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

/**
    The tablet/stage remote.

    Everything is sized for a finger: a 44 px floor on every control, two
    enormous patch-step buttons, a scrollable set list and a panic button that
    cannot be missed. The layout is a single column below 800 px and a
    two-column performance layout above it.

    The stage header, now-playing card, set list, transport, panic and
    reconnect banner live in their own components; this file keeps the page
    shell and wiring.
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

            <ReconnectBanner connection={connection} link={link} />

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
                    <RemoteTransport
                        transport={model.transport}
                        onTogglePlay={onTogglePlay}
                        onStop={onStop}
                    />
                }
                panic={<RemotePanic onPanic={onPanic} />}
            />
        </main>
    );
}
