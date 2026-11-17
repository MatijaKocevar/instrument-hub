import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactElement,
} from "react";

import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { MidiBridgeStage } from "../midiBridge";
import { useMidiBridgeController } from "../midiBridge/useMidiBridgeController";
import { codeFromSearch, PairingGate } from "../pairing";
import { useConnectionToasts } from "../toasts";
import { RemoteStage } from "./RemoteStage";

type RemoteView = "stage" | "midi";

function viewFromSearch(search: string): RemoteView {
  const query = search.startsWith("?") ? search.slice(1) : search;

  for (const part of query.split("&")) {
    const separator = part.indexOf("=");

    if (separator < 0) continue;

    if (decodeURIComponent(part.slice(0, separator)) !== "view") continue;

    return decodeURIComponent(part.slice(separator + 1)) === "midi"
      ? "midi"
      : "stage";
  }

  return "stage";
}

export function RemoteRoute(): ReactElement {
  const app = useAppState();
  const store = useAppStore();
  const midiBridge = useMidiBridgeController({
    baseUrl: store.baseUrl,
    token: store.token,
  });

  useConnectionToasts(app.phase, app.error);
  const initialCode = useMemo(
    () =>
      typeof window === "undefined"
        ? null
        : codeFromSearch(window.location.search),
    [],
  );
  const attempted = useRef<string | null>(null);
  const [view, setView] = useState<RemoteView>(() =>
    typeof window === "undefined"
      ? "stage"
      : viewFromSearch(window.location.search),
  );

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

  useEffect(() => {
    if (app.unauthorized || typeof window === "undefined") return;

    const url = new URL(window.location.href);

    if (!url.searchParams.has("code")) return;

    url.searchParams.delete("code");
    window.history.replaceState({}, "", url);
  }, [app.unauthorized]);

  const show = useCallback((next: RemoteView): void => {
    setView(next);

    if (typeof window === "undefined") return;

    const url = new URL(window.location.href);

    if (next === "midi") url.searchParams.set("view", "midi");
    else url.searchParams.delete("view");

    window.history.replaceState({}, "", url);
  }, []);

  if (app.unauthorized) {
    return (
      <PairingGate
        pairing={app.pairing}
        busy={app.pairingBusy}
        initialCode={initialCode}
        onPair={(code) => {
          void store.pair(code);
        }}
      />
    );
  }

  if (view === "midi")
    return (
      <MidiBridgeStage
        controller={midiBridge}
        onBack={() => {
          show("stage");
        }}
      />
    );

  return (
    <RemoteStage
      midiOn={midiBridge.bridgeOn}
      onOpenMidi={() => {
        show("midi");
      }}
    />
  );
}
