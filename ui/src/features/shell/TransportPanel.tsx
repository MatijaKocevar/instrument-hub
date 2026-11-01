import type { ReactElement } from "react";

import type { ControlTransport } from "../../api/client";

function formatBpm(bpm: number): string {
  return `${Number.isInteger(bpm) ? bpm.toFixed(0) : bpm.toFixed(1)} BPM`;
}

export function TransportPanel({
  transport,
}: {
  readonly transport: ControlTransport;
}): ReactElement {
  return (
    <article className="panel">
      <h2 className="panel__title">Transport</h2>
      <dl className="panel__meta">
        <div>
          <dt>State</dt>
          <dd data-testid="transport-state">
            {transport.playing ? "Playing" : "Stopped"}
          </dd>
        </div>
        <div>
          <dt>Tempo</dt>
          <dd data-testid="transport-bpm">{formatBpm(transport.bpm)}</dd>
        </div>
        <div>
          <dt>Position</dt>
          <dd>{transport.ppq.toFixed(2)} beats</dd>
        </div>
        <div>
          <dt>Samples</dt>
          <dd>{transport.positionSamples.toLocaleString("en-US")}</dd>
        </div>
      </dl>
    </article>
  );
}
