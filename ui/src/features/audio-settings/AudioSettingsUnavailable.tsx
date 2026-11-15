import type { ReactElement } from "react";

import "./audio-settings.css";

export interface AudioSettingsUnavailableProps {
  readonly onRetry?: (() => void) | undefined;
}

export function AudioSettingsUnavailable({
  onRetry,
}: AudioSettingsUnavailableProps): ReactElement {
  return (
    <section
      className="audio-settings"
      aria-label="Audio settings"
      data-testid="audio-settings"
      data-unavailable="true"
    >
      <header className="audio-settings__header">
        <div>
          <p className="audio-settings__eyebrow">Engine</p>
          <h2 className="audio-settings__title">Audio settings</h2>
        </div>
      </header>

      <div className="audio-settings__unavailable" role="status">
        <span>Audio settings unavailable.</span>
        {onRetry === undefined ? null : (
          <button
            className="audio-settings__unavailable-retry"
            type="button"
            data-testid="audio-retry"
            onClick={onRetry}
          >
            Retry
          </button>
        )}
      </div>
    </section>
  );
}
