import { useEffect, useState, useSyncExternalStore } from "react";

import { MeterStore } from "../grid";
import { MeterLink, type RemoteLinkSnapshot } from "./MeterLink";

export function useRemoteLink(url: string): RemoteLinkSnapshot {
  const [link] = useState(
    () =>
      new MeterLink(
        new MeterStore({
          url,
          reconnectBaseMs: 250,
          reconnectMaxMs: 2000,
        }),
      ),
  );

  useEffect(() => {
    link.start();

    return () => {
      link.stop();
    };
  }, [link]);

  return useSyncExternalStore(
    link.subscribe,
    link.getSnapshot,
    link.getSnapshot,
  );
}
