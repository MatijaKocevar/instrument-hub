import type { ConnectionPhase } from "../../state/store";

/** User-facing label for each connection phase. */
export const PHASE_LABEL: Record<ConnectionPhase, string> = {
    idle: "Idle",
    connecting: "Connecting",
    ready: "Connected",
    offline: "Offline",
};
