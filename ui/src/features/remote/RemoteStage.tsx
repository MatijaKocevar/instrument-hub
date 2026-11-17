import { useEffect, useMemo, useState, type ReactElement } from "react";

import { metersUrlFor } from "../grid";
import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { useRemoteLinkToasts } from "../toasts";
import { RemoteView } from "./RemoteView";
import { remoteFromState, stepPatch } from "./model";
import {
  loadRemoteContrast,
  nextContrast,
  saveRemoteContrast,
  type ContrastStorage,
  type RemoteContrast,
} from "./theme";
import { useRemoteLink } from "./useRemoteLink";
import { useWakeLock } from "./useWakeLock";

function contrastStorage(): ContrastStorage | null {
  return typeof localStorage === "undefined" ? null : localStorage;
}

export function RemoteStage({
  onOpenMidi,
  midiOn = false,
}: {
  readonly onOpenMidi?: () => void;
  readonly midiOn?: boolean;
}): ReactElement {
  const app = useAppState();
  const store = useAppStore();
  const model = useMemo(() => remoteFromState(app.state), [app.state]);

  const metersUrl = useMemo(
    () => metersUrlFor(store.baseUrl, store.token),
    [store.baseUrl, store.token],
  );
  const link = useRemoteLink(metersUrl);
  const wakeLock = useWakeLock();

  useRemoteLinkToasts(app.phase, link);

  const [contrast, setContrast] = useState<RemoteContrast>(() =>
    loadRemoteContrast(contrastStorage()),
  );

  useEffect(() => {
    const onVisibilityChange = (): void => {
      if (document.visibilityState === "visible") void store.refresh();
    };

    document.addEventListener("visibilitychange", onVisibilityChange);

    return () => {
      document.removeEventListener("visibilitychange", onVisibilityChange);
    };
  }, [store]);

  const toggleContrast = (): void => {
    setContrast((current) => {
      const next = nextContrast(current);
      saveRemoteContrast(contrastStorage(), next);

      return next;
    });
  };

  const step = (direction: 1 | -1): void => {
    const slot = stepPatch(model, direction);

    if (slot !== null) void store.armSlot(slot);
  };

  return (
    <RemoteView
      model={model}
      connection={app.phase}
      loading={app.state === null}
      link={link}
      contrast={contrast}
      wakeLock={wakeLock}
      onToggleContrast={toggleContrast}
      onSelect={(slot) => {
        void store.armSlot(slot);
      }}
      onPrevious={() => {
        step(-1);
      }}
      onNext={() => {
        step(1);
      }}
      onTogglePlay={() => {
        void store.updateTransport({ playing: !model.transport.playing });
      }}
      onStop={() => {
        void store.updateTransport({
          playing: false,
          ppq: 0,
          positionSamples: 0,
        });
      }}
      onPanic={() => {
        void store.panic();
      }}
      onOpenMidi={onOpenMidi}
      midiOn={midiOn}
    />
  );
}
