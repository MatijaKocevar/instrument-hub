import type { ReactElement } from "react";

import { SettingsPage } from "../settings";
import type { ShellData } from "./useShellData";

export function SettingsRoute({
  shell,
}: {
  readonly shell: ShellData;
}): ReactElement {
  const { app, state, store } = shell;

  return (
    <SettingsPage
      pairing={app.pairing}
      token={store.token}
      pairingBusy={app.pairingBusy}
      connection={app.phase}
      baseUrl={store.baseUrl}
      application={state?.application ?? null}
      schemaVersion={state?.schemaVersion ?? null}
      uptimeMs={state?.uptimeMs ?? null}
      loading={state === null}
      onToggleLan={(enabled) => {
        void store.toggleLan(enabled);
      }}
      onForget={() => {
        void store.forgetPairing();
      }}
      onPair={(code) => {
        void store.pair(code);
      }}
    />
  );
}
