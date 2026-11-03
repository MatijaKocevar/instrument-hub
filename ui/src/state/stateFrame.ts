import type { ControlState } from "../api/client";
import { isControlState } from "../api/state";

export interface StatePushFrame {
  readonly sequence: number;
  readonly revision: number;
  readonly kind: string;
  readonly state: ControlState;
}

function numberOrZero(value: unknown): number {
  return typeof value === "number" && Number.isFinite(value) ? value : 0;
}

export function parseStatePushFrame(value: unknown): StatePushFrame | null {
  let candidate = value;

  if (typeof candidate === "string") {
    try {
      candidate = JSON.parse(candidate);
    } catch {
      return null;
    }
  }

  if (candidate === null || typeof candidate !== "object") return null;

  const frame = candidate as {
    type?: unknown;
    sequence?: unknown;
    revision?: unknown;
    event?: { kind?: unknown };
    state?: unknown;
  };

  if (frame.type !== "state" || !isControlState(frame.state)) return null;

  return {
    sequence: numberOrZero(frame.sequence),
    revision:
      typeof frame.revision === "number"
        ? frame.revision
        : frame.state.revision,
    kind: typeof frame.event?.kind === "string" ? frame.event.kind : "state",
    state: frame.state,
  };
}
