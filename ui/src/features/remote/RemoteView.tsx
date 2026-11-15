import type { ReactElement } from "react";

import { linkPhase, type RemoteLinkSnapshot } from "./MeterLink";
import type { RemoteConnection, RemoteModel } from "./model";
import { NowPlayingCard } from "./NowPlayingCard";
import { RemoteHeader } from "./RemoteHeader";
import { RemoteLayout } from "./RemoteLayout";
import { RemotePanic } from "./RemotePanic";
import { RemoteTransport } from "./RemoteTransport";
import { SetList } from "./SetList";
import type { RemoteContrast } from "./theme";
import type { WakeLockStatus } from "./wakeLockTypes";
import "./remote.css";

export type { RemoteConnection } from "./model";

export interface RemoteViewProps {
  readonly model: RemoteModel;
  readonly connection?: RemoteConnection;
  readonly loading?: boolean;
  readonly link?: RemoteLinkSnapshot | null;
  readonly contrast?: RemoteContrast;
  readonly wakeLock?: WakeLockStatus;
  readonly onToggleContrast?: () => void;
  readonly onSelect?: (slot: number) => void;
  readonly onPrevious?: () => void;
  readonly onNext?: () => void;
  readonly onTogglePlay?: () => void;
  readonly onStop?: () => void;
  readonly onPanic?: () => void;
}

export function RemoteView({
  model,
  connection = "idle",
  loading = false,
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
}: RemoteViewProps): ReactElement {
  const playing = model.transport.playing;

  return (
    <main
      className="remote"
      aria-label="Stage remote"
      data-testid="remote"
      data-theme="stage"
      data-contrast={contrast}
      data-connection={connection}
      data-link={linkPhase(link)}
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
          <SetList model={model} loading={loading} onSelect={onSelect} />
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
