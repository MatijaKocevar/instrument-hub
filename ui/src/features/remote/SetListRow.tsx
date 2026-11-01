import type { ReactElement } from "react";

import { formatLatency, pad2 } from "./format";
import type { RemotePatch } from "./model";

export interface SetListRowProps {
  readonly patch: RemotePatch;
  readonly index: number;
  readonly armed: boolean;
  readonly onSelect: ((slot: number) => void) | undefined;
}

export function SetListRow({
  patch,
  index,
  armed,
  onSelect,
}: SetListRowProps): ReactElement {
  return (
    <li className="remote__set-row">
      <button
        className={`remote__set-item${armed ? " remote__set-item--armed" : ""}`}
        type="button"
        data-testid="remote-set-item"
        data-slot={patch.slot}
        data-armed={armed ? "true" : "false"}
        aria-current={armed ? "true" : undefined}
        aria-pressed={armed}
        onClick={
          onSelect === undefined
            ? undefined
            : () => {
                onSelect(patch.slot);
              }
        }
      >
        <span className="remote__set-index">{pad2(index + 1)}</span>
        <span className="remote__set-name">{patch.name}</span>
        <span className="remote__set-latency">
          {formatLatency(patch.latencyMs)}
        </span>
        {armed ? <span className="remote__set-badge">Now</span> : null}
      </button>
    </li>
  );
}
