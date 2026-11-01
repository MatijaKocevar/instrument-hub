import type { ReactElement } from "react";

import { formatBpm, formatPosition, type RemoteModel } from "./model";

export interface RemoteTransportProps {
  readonly transport: RemoteModel["transport"];
  readonly onTogglePlay: (() => void) | undefined;
  readonly onStop: (() => void) | undefined;
}

export function RemoteTransport({
  transport,
  onTogglePlay,
  onStop,
}: RemoteTransportProps): ReactElement {
  const playing = transport.playing;

  return (
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
        {formatBpm(transport.bpm)}
      </p>
      <p className="remote__position" data-testid="remote-position">
        Bar {formatPosition(transport.ppq)}
        <span className="remote__position-detail">
          {transport.positionSamples.toLocaleString("en-US")} samples
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
  );
}
