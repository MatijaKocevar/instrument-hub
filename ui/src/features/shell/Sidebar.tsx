import type { ReactElement } from "react";

import { NavLink } from "./NavLink";
import { PHASE_LABEL } from "./phaseLabel";
import {
  SETTINGS_ITEM,
  WORKSPACE_ITEMS,
  type ShellNavProps,
} from "./navigation";

export function Sidebar({
  route,
  navigate,
  phase,
  lanEnabled,
  paired,
  version,
}: ShellNavProps): ReactElement {
  return (
    <aside
      className="shell__sidebar"
      data-testid="app-sidebar"
      data-collapsed="false"
    >
      <div className="shell__brand">
        <span className="shell__mark" aria-hidden="true">
          IH
        </span>
        <span className="shell__brand-text">
          <span className="shell__brand-name" data-testid="sidebar-title">
            Instrument Hub
          </span>
          <span className="shell__brand-lede">Audio workspace</span>
        </span>
      </div>

      <nav className="shell__nav" aria-label="Workspace" data-testid="app-nav">
        <p className="shell__nav-title">Workspace</p>
        {WORKSPACE_ITEMS.map((item) => (
          <NavLink
            key={item.route}
            item={item}
            route={route}
            navigate={navigate}
          />
        ))}
      </nav>

      <div className="shell__account" data-testid="settings-group">
        <p className="shell__nav-title">Settings &amp; Account</p>
        <nav
          className="shell__nav"
          aria-label="Settings and account"
          data-testid="settings-nav"
        >
          <NavLink item={SETTINGS_ITEM} route={route} navigate={navigate} />
        </nav>

        <p
          className="shell__session"
          data-testid="sidebar-session"
          data-phase={phase}
          data-lan={lanEnabled ? "true" : "false"}
          data-paired={paired ? "true" : "false"}
        >
          <span
            className={`shell__dot shell__dot--${phase}`}
            aria-hidden="true"
          />
          {PHASE_LABEL[phase]}
          <span className="shell__session-detail">
            {lanEnabled ? "LAN on" : "LAN off"}
          </span>
        </p>
        <p className="shell__version" data-testid="sidebar-version">
          v{version}
        </p>
      </div>
    </aside>
  );
}
