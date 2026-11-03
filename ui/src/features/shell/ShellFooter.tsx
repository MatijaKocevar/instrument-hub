import type { ReactElement } from "react";

import type { ShellRoute } from "../../state/router";
import { NavLink } from "./NavLink";
import { AUDIO_ITEM } from "./navigation";
import { PHASE_LABEL } from "./phaseLabel";
import { ProjectFooter } from "./ProjectFooter";
import type { ShellData } from "./useShellData";

export function ShellFooter({
  shell,
  route,
  navigate,
}: {
  readonly shell: ShellData;
  readonly route: ShellRoute;
  readonly navigate: (path: string) => void;
}): ReactElement {
  const { app, state, armedSlot, version } = shell;

  return (
    <footer className="app__footer" data-testid="app-footer">
      <ProjectFooter shell={shell} />
      <span
        className="app__status"
        data-testid="app-status"
        data-phase={app.phase}
      >
        {state === null
          ? "Waiting for device state"
          : `${PHASE_LABEL[app.phase]} · ${state.slots.length} slots · armed ${armedSlot < 0 ? "none" : armedSlot + 1}`}
      </span>
      <span className="app__footer-actions">
        <NavLink
          item={AUDIO_ITEM}
          route={route}
          navigate={navigate}
          variant="footer"
        />
        <span className="app__version">v{version}</span>
      </span>
    </footer>
  );
}
