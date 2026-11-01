import { useEffect, useState, type ReactElement } from "react";

import type { ControlPluginList } from "../../api/client";
import { filterPlugins, type PluginAssignment } from "./model";
import { PluginList } from "./PluginList";
import { PluginsHeader } from "./PluginsHeader";
import { PluginsToolbar } from "./PluginsToolbar";
import "./plugins.css";

const SCAN_POLL_MS = 400;

const MAX_SCAN_POLLS = 150;

export interface PluginsPageProps {
  readonly inventory: ControlPluginList | null;
  readonly assignments?: readonly PluginAssignment[];
  readonly error?: string | null;
  readonly busy?: boolean;
  readonly initialSearch?: string;
  readonly onRescan?: () => void;
  readonly onReload?: () => void;
}

export function PluginsPage({
  inventory,
  assignments = [],
  error = null,
  busy = false,
  initialSearch = "",
  onRescan,
  onReload,
}: PluginsPageProps): ReactElement {
  const [search, setSearch] = useState(initialSearch);
  const [scanPolls, setScanPolls] = useState(0);

  const scanning = inventory?.scanning === true;
  const all = inventory?.plugins ?? [];
  const shown = filterPlugins(all, search);
  const total = inventory?.total ?? all.length;

  useEffect(() => {
    if (
      !scanning ||
      busy ||
      onReload === undefined ||
      scanPolls >= MAX_SCAN_POLLS
    )
      return undefined;

    const timer = setTimeout(() => {
      setScanPolls((count) => count + 1);
      onReload();
    }, SCAN_POLL_MS);

    return () => {
      clearTimeout(timer);
    };
  }, [scanning, busy, scanPolls, onReload]);

  const rescan = (): void => {
    setScanPolls(0);
    onRescan?.();
  };

  return (
    <section
      className="plugins"
      data-testid="plugins-page"
      data-count={shown.length}
      data-total={total}
      data-scanning={scanning ? "true" : "false"}
      data-scanned={inventory?.scanned === true ? "true" : "false"}
      aria-busy={busy || scanning ? "true" : "false"}
    >
      <PluginsHeader inventory={inventory} shown={shown.length} total={total} />

      <PluginsToolbar
        search={search}
        busy={busy}
        scanning={scanning}
        onSearch={setSearch}
        onClear={() => {
          setSearch("");
        }}
        onRescan={rescan}
      />

      {error !== null ? (
        <div className="plugins__error" data-testid="plugin-error" role="alert">
          <span>{error}</span>
          {onReload !== undefined ? (
            <button
              className="plugins__button plugins__button--small"
              type="button"
              data-testid="plugin-retry"
              onClick={onReload}
            >
              Retry
            </button>
          ) : null}
        </div>
      ) : null}

      {inventory === null && error !== null ? null : (
        <PluginList
          loading={inventory === null}
          plugins={shown}
          search={search}
          assignments={assignments}
        />
      )}

      <p className="plugins__note" data-testid="plugins-note">
        This page only reads the scan. “Used by” counts patches whose instrument
        source is this plug-in; to change a patch's instrument or insert an
        effect, open that patch on the Instrument page.
      </p>
    </section>
  );
}
