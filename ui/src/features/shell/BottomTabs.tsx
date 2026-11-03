import type { ReactElement } from "react";

import { NavLink } from "./NavLink";
import {
  AUDIO_ITEM,
  SETTINGS_ITEM,
  WORKSPACE_ITEMS,
  type ShellNavProps,
} from "./navigation";

export function BottomTabs({ route, navigate }: ShellNavProps): ReactElement {
  return (
    <nav
      className="shell__tabs"
      aria-label="Workspace"
      data-testid="app-bottom-tabs"
      data-collapsed="true"
    >
      {[...WORKSPACE_ITEMS, AUDIO_ITEM, SETTINGS_ITEM].map((item) => (
        <NavLink
          key={item.route}
          item={item}
          route={route}
          navigate={navigate}
          variant="tab"
        />
      ))}
    </nav>
  );
}
