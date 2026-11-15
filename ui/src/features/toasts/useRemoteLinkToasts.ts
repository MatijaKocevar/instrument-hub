import { useEffect, useRef } from "react";

import { useAppStore } from "../../state/AppStoreContext";
import type { RemoteLinkSnapshot } from "../remote/MeterLink";
import type { RemoteConnection } from "../remote/model";

const KEY = "remote-link";

export function useRemoteLinkToasts(
  connection: RemoteConnection,
  link: RemoteLinkSnapshot | null,
): void {
  const store = useAppStore();
  const wasLost = useRef(false);
  const lost =
    connection !== "offline" &&
    link !== null &&
    link.everConnected &&
    !link.connected;

  useEffect(() => {
    if (connection === "offline") {
      if (wasLost.current) store.dismissToastKey(KEY);

      wasLost.current = false;
      return;
    }

    if (lost) {
      wasLost.current = true;
      store.notify({
        severity: "warning",
        title: "Live link lost",
        detail: "Reconnecting automatically — the set list stays usable.",
        key: KEY,
        sticky: true,
      });
      return;
    }

    if (!wasLost.current) return;

    wasLost.current = false;
    store.resolveToast(KEY, {
      severity: "success",
      title: "Live link restored",
      key: KEY,
    });
  }, [connection, lost, store]);
}
