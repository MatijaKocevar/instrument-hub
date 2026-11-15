import { useEffect, useRef } from "react";

import type { ControlPatch } from "../../api/client";
import { useAppStore } from "../../state/AppStoreContext";

interface PatchError {
  readonly slot: number;
  readonly error: string;
}

export function usePatchStatusToasts(patch: ControlPatch | null): void {
  const store = useAppStore();
  const previous = useRef<PatchError | null>(null);

  useEffect(() => {
    const prior = previous.current;
    const slot = patch?.slot ?? -1;

    if (prior !== null && prior.slot !== slot)
      store.dismissToastKey(`patch:${String(prior.slot)}`);

    previous.current =
      patch === null ? null : { slot, error: patch.error ?? "" };

    if (patch === null) return;

    const error = patch.error ?? "";

    if (error.length > 0) {
      if (prior === null || prior.slot !== slot || prior.error !== error) {
        store.notify({
          severity: "error",
          title: "Instrument problem",
          detail: error,
          key: `patch:${String(slot)}`,
          sticky: true,
        });
      }
      return;
    }

    if (prior !== null && prior.slot === slot && prior.error.length > 0) {
      store.resolveToast(`patch:${String(slot)}`, {
        severity: "success",
        title: "Instrument ready",
        key: `patch:${String(slot)}`,
      });
    }
  }, [patch, store]);
}
