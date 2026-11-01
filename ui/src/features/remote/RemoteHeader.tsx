import type { ReactElement } from "react";

import type { RemoteConnection } from "./model";
import { contrastLabel, type RemoteContrast } from "./theme";
import type { WakeLockStatus } from "./wakeLockTypes";

const CONNECTION_LABEL: Record<RemoteConnection, string> = {
  idle: "Idle",
  connecting: "Connecting",
  ready: "Live",
  offline: "Offline",
};

export interface RemoteHeaderProps {
  readonly applicationName: string;
  readonly connection: RemoteConnection;
  readonly contrast: RemoteContrast;
  readonly wakeLock: WakeLockStatus;
  readonly onToggleContrast: (() => void) | undefined;
}

export function RemoteHeader({
  applicationName,
  connection,
  contrast,
  wakeLock,
  onToggleContrast,
}: RemoteHeaderProps): ReactElement {
  return (
    <header className="remote__header">
      <div className="remote__heading">
        <p className="remote__eyebrow">Stage remote</p>
        <h1 className="remote__title">{applicationName}</h1>
      </div>

      <div className="remote__header-actions">
        {wakeLock === "active" ? (
          <span
            className="remote__wake"
            data-testid="remote-wake-lock"
            data-state="active"
          >
            Screen on
          </span>
        ) : wakeLock === "denied" || wakeLock === "unavailable" ? (
          <span
            className="remote__wake remote__wake--off"
            data-testid="remote-wake-lock"
            data-state={wakeLock}
          >
            Screen may sleep
          </span>
        ) : null}

        <button
          className="remote__contrast"
          type="button"
          data-testid="remote-contrast"
          data-contrast={contrast}
          aria-pressed={contrast === "high"}
          aria-label="Toggle the high-contrast stage theme"
          onClick={onToggleContrast}
        >
          {contrastLabel(contrast)}
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
  );
}
