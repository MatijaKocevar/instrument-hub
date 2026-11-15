import { useEffect, useRef } from "react";

import type { ControlPluginList } from "../../api/client";
import { useAppStore } from "../../state/AppStoreContext";

const SCAN_KEY = "plugin-scan";
const ERROR_KEY = "plugin-scan-error";

export function usePluginStatusToasts(
  inventory: ControlPluginList | null,
): void {
  const store = useAppStore();
  const previousScanning = useRef<boolean | null>(null);
  const previousError = useRef("");

  useEffect(() => {
    const priorScanning = previousScanning.current;

    if (inventory === null) {
      previousScanning.current = null;
      return;
    }

    const scanning = inventory.scanning;

    previousScanning.current = scanning;

    if (scanning && priorScanning === false) {
      store.notify({
        severity: "info",
        title: "Scanning plug-ins",
        detail: "The scan continues in the background.",
        key: SCAN_KEY,
      });
    } else if (!scanning && priorScanning === true) {
      store.resolveToast(SCAN_KEY, {
        severity: "success",
        title: "Plug-in scan finished",
        detail: `${String(inventory.total)} plug-ins`,
        key: SCAN_KEY,
      });
    }

    const priorError = previousError.current;

    previousError.current = inventory.error;

    if (inventory.error.length > 0) {
      if (priorError !== inventory.error) {
        store.notify({
          severity: "error",
          title: "Plug-in scan problem",
          detail: inventory.error,
          key: ERROR_KEY,
          sticky: true,
        });
      }
    } else if (priorError.length > 0) {
      store.resolveToast(ERROR_KEY, {
        severity: "success",
        title: "Plug-in scan recovered",
        key: ERROR_KEY,
      });
    }
  }, [inventory, store]);
}
