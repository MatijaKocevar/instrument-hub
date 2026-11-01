import { useEffect, useMemo, useRef, type ReactElement } from "react";

import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { codeFromSearch, PairingGate } from "../pairing";
import { RemoteStage } from "./RemoteStage";

export function RemoteRoute(): ReactElement {
  const app = useAppState();
  const store = useAppStore();
  const initialCode = useMemo(
    () =>
      typeof window === "undefined"
        ? null
        : codeFromSearch(window.location.search),
    [],
  );
  const attempted = useRef<string | null>(null);

  useEffect(() => {
    if (
      initialCode === null ||
      store.token !== null ||
      attempted.current === initialCode
    )
      return;

    attempted.current = initialCode;
    void store.pair(initialCode);
  }, [initialCode, store]);

  if (app.unauthorized) {
    return (
      <PairingGate
        pairing={app.pairing}
        busy={app.pairingBusy}
        error={app.pairingError ?? app.error}
        initialCode={initialCode}
        onPair={(code) => {
          void store.pair(code);
        }}
      />
    );
  }

  return <RemoteStage />;
}
