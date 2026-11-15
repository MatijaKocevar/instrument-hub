import { useEffect, useRef } from "react";

import { useAppStore } from "../../state/AppStoreContext";
import type { GridPageStatus } from "../grid/model";

const KEY = "instruments";

export function useGridStatusToasts(page: GridPageStatus | null): void {
  const store = useAppStore();
  const previous = useRef<string | null>(null);

  useEffect(() => {
    if (page === null) {
      previous.current = null;
      return;
    }

    const status = page.status;
    const prior = previous.current;

    previous.current = status;

    if (status === "recovering") {
      store.notify({
        severity: "warning",
        title: "Restoring instruments…",
        detail:
          page.error.length > 0
            ? page.error
            : "The shared plug-in host is restarting and restoring every track.",
        key: KEY,
        sticky: true,
      });
      return;
    }

    if (status === "failed") {
      store.notify({
        severity: "error",
        title: "Instruments unavailable",
        detail:
          page.error.length > 0
            ? page.error
            : "Reload the page or reopen the project.",
        key: KEY,
        sticky: true,
      });
      return;
    }

    if (prior === "recovering" || prior === "failed") {
      store.resolveToast(KEY, {
        severity: "success",
        title: "Instruments ready",
        key: KEY,
      });
    }
  }, [page?.error, page?.status, store]);
}
