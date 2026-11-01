import { useEffect, useState, useSyncExternalStore } from "react";

import { MeterStore, type MeterStoreOptions } from "./MeterStore";
import type { MeterSnapshot } from "./meterFrame";

export function useMeters(options: MeterStoreOptions): MeterSnapshot {
  const [store] = useState(() => new MeterStore(options));

  useEffect(() => {
    store.start();

    return () => {
      store.stop();
    };
  }, [store]);

  return useSyncExternalStore(
    store.subscribe,
    store.getSnapshot,
    store.getSnapshot,
  );
}
