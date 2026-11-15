import { useEffect, useRef } from "react";

import { useAppStore } from "../../state/AppStoreContext";
import type { ConnectionPhase } from "../../state/snapshot";

export function useConnectionToasts(
  phase: ConnectionPhase,
  error: string | null,
): void {
  const store = useAppStore();
  const wasOffline = useRef(false);

  useEffect(() => {
    if (phase === "offline") {
      wasOffline.current = true;
      store.notify({
        severity: "error",
        title: "Engine unreachable",
        detail: error ?? "The control API is not responding.",
        key: "connection",
        sticky: true,
      });
      return;
    }

    if (phase === "ready" && wasOffline.current) {
      wasOffline.current = false;
      store.notify({
        severity: "success",
        title: "Reconnected",
        detail: "The engine is reachable again.",
        key: "connection",
      });
    }
  }, [error, phase, store]);
}
