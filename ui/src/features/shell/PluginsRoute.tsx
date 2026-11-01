import type { ReactElement } from "react";

import { PluginsPage } from "../plugins";
import type { ShellData } from "./useShellData";

export function PluginsRoute({
  shell,
}: {
  readonly shell: ShellData;
}): ReactElement {
  const { app, store, pluginAssignments } = shell;

  return (
    <PluginsPage
      inventory={app.plugins}
      assignments={pluginAssignments}
      error={app.pluginError}
      busy={app.pluginBusy}
      onRescan={() => {
        void store.rescanPlugins();
      }}
      onReload={() => {
        void store.loadPlugins();
      }}
    />
  );
}
