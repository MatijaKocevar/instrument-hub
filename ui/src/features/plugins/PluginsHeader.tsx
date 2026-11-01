import type { ReactElement } from "react";

import type { ControlPluginList } from "../../api/client";
import { formatScanTime, pluginCountLabel } from "./model";

interface PluginsHeaderProps {
  readonly inventory: ControlPluginList | null;
  readonly shown: number;
  readonly total: number;
}

function scanStateLabel(inventory: ControlPluginList | null): string {
  if (inventory === null) return "Reading the inventory";

  if (inventory.scanning) return "Scanning…";

  if (!inventory.scanned) return "No scan has run yet";

  return `Last scan ${formatScanTime(inventory.lastScanMs)}`;
}

export function PluginsHeader({
  inventory,
  shown,
  total,
}: PluginsHeaderProps): ReactElement {
  const scanning = inventory?.scanning === true;

  return (
    <header className="plugins__head">
      <div className="plugins__intro">
        <p className="plugins__eyebrow">Plugins</p>
        <h2 className="plugins__title">Installed plugins</h2>
        <p className="plugins__lede">
          Everything the VST3 scanner found. Search by name, vendor, category or
          bundle path; assignment stays on each patch.
        </p>
      </div>

      <div className="plugins__status">
        <span className="plugins__count" data-testid="plugin-count">
          {pluginCountLabel(shown, total)}
        </span>
        <span
          className={`plugins__scan${scanning ? " plugins__scan--running" : ""}`}
          data-testid="plugin-scan-state"
          data-scanning={scanning ? "true" : "false"}
          data-scanned={inventory?.scanned === true ? "true" : "false"}
        >
          {scanStateLabel(inventory)}
        </span>
      </div>
    </header>
  );
}
