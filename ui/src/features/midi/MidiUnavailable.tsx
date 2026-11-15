import type { ReactElement } from "react";

import "./midi.css";

export interface MidiUnavailableProps {
  readonly onRetry?: (() => void) | undefined;
}

export function MidiUnavailable({
  onRetry,
}: MidiUnavailableProps): ReactElement {
  return (
    <section
      className="midi"
      aria-label="MIDI inputs"
      data-testid="midi-panel"
      data-unavailable="true"
    >
      <header className="midi__head">
        <div>
          <p className="midi__eyebrow">Controllers</p>
          <h2 className="midi__title">MIDI</h2>
        </div>
      </header>

      <div className="midi__unavailable" role="status">
        <span>MIDI inputs unavailable.</span>
        {onRetry === undefined ? null : (
          <button
            className="midi__retry"
            type="button"
            data-testid="midi-retry"
            onClick={onRetry}
          >
            Retry
          </button>
        )}
      </div>
    </section>
  );
}
