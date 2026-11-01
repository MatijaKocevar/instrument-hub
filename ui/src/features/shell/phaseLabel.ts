import type { ConnectionPhase } from "../../state/store";

export const PHASE_LABEL: Record<ConnectionPhase, string> = {
  idle: "Idle",
  connecting: "Connecting",
  ready: "Connected",
  offline: "Offline",
};
