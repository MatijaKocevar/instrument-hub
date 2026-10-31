import { useEffect, useState, type ReactElement } from "react";

import type { ControlPlugin, ControlPluginList } from "../../api/client";
import {
    filterPlugins,
    formatScanTime,
    pluginCountLabel,
    pluginKind,
    usedByPlugin,
    type PluginAssignment,
} from "./model";
import "./plugins.css";

/** How often the page re-reads the inventory while a scan is running. */
const SCAN_POLL_MS = 400;

/** Give up polling after a minute; the page still offers a manual refresh. */
const MAX_SCAN_POLLS = 150;

export interface PluginsPageProps
{
    /** Latest `GET /plugins`, or null before the first successful request. */
    readonly inventory: ControlPluginList | null;
    /** Patches that use a plug-in, derived from `/state`. */
    readonly assignments?: readonly PluginAssignment[];
    /** Message from a failed inventory load or rescan. */
    readonly error?: string | null;
    /** True while a rescan or inventory request is in flight. */
    readonly busy?: boolean;
    /** Search prefilled when the page mounts (used by the snapshot test). */
    readonly initialSearch?: string;
    /** Start a background scan of the VST3 folders. */
    readonly onRescan?: () => void;
    /** Re-read the inventory, used by Retry and by scan polling. */
    readonly onReload?: () => void;
}

function padSlot (slot: number): string
{
    return String (slot + 1).padStart (2, "0");
}

function scanStateLabel (inventory: ControlPluginList | null): string
{
    if (inventory === null)
        return "Reading the inventory";

    if (inventory.scanning)
        return "Scanning…";

    if (! inventory.scanned)
        return "No scan has run yet";

    return `Last scan ${formatScanTime (inventory.lastScanMs)}`;
}

function PluginCard ({ plugin, assignments }: {
    readonly plugin: ControlPlugin;
    readonly assignments: readonly PluginAssignment[];
}): ReactElement
{
    const usage = usedByPlugin (assignments, plugin);

    return (
        <li
            className="plugins__card"
            data-testid="plugin-row"
            data-plugin-id={plugin.id}
            data-name={plugin.name}
            data-vendor={plugin.vendor}
            data-kind={pluginKind (plugin)}
            data-instrument={plugin.isInstrument ? "true" : "false"}
            data-effect={plugin.isEffect ? "true" : "false"}
            data-used-by={usage.length}
        >
            <header className="plugins__card-head">
                <div className="plugins__identity">
                    <h3 className="plugins__name" data-testid="plugin-name">
                        {plugin.name}
                    </h3>
                    <p className="plugins__vendor" data-testid="plugin-vendor">
                        {plugin.vendor.length > 0 ? plugin.vendor : "Unknown vendor"}
                    </p>
                </div>
                <span
                    className={`plugins__kind${plugin.isInstrument ? " plugins__kind--instrument" : ""}`}
                    data-testid="plugin-kind"
                    data-instrument={plugin.isInstrument ? "true" : "false"}
                    data-effect={plugin.isEffect ? "true" : "false"}
                >
                    {pluginKind (plugin)}
                </span>
            </header>

            {plugin.description.length > 0
                ? <p className="plugins__description" data-testid="plugin-description">
                      {plugin.description}
                  </p>
                : null}

            <dl className="plugins__meta">
                <div>
                    <dt>Version</dt>
                    <dd data-testid="plugin-version">
                        {plugin.version.length > 0 ? plugin.version : "—"}
                    </dd>
                </div>
                <div>
                    <dt>Category</dt>
                    <dd data-testid="plugin-category">
                        {plugin.category.length > 0 ? plugin.category : "—"}
                    </dd>
                </div>
                <div>
                    <dt>Format</dt>
                    <dd data-testid="plugin-format">{plugin.format}</dd>
                </div>
            </dl>

            <p className="plugins__path" data-testid="plugin-path" title={plugin.path}>
                <span className="plugins__path-label">Bundle</span>
                <code>{plugin.path}</code>
            </p>

            {usage.length > 0
                ? <div
                      className="plugins__used"
                      data-testid="plugin-used-by"
                      data-count={usage.length}
                  >
                      <span className="plugins__used-label">Used by</span>
                      <ul className="plugins__used-list">
                          {usage.map (assignment => (
                              <li
                                  className="plugins__used-entry"
                                  data-testid="plugin-used-by-entry"
                                  data-slot={assignment.slot}
                                  data-patch={assignment.patchName}
                                  key={assignment.slot}
                              >
                                  <span className="plugins__used-patch">
                                      {assignment.patchName}
                                  </span>
                                  <span className="plugins__used-meta">
                                      slot {padSlot (assignment.slot)} · {assignment.familyName}
                                  </span>
                              </li>
                          ))}
                      </ul>
                  </div>
                : <p className="plugins__unused" data-testid="plugin-unused">
                      Not assigned to a patch.
                  </p>}
        </li>
    );
}

/**
    Read-only inventory of installed VST3s.

    The page lists what the scanner found, filters it locally as the user types,
    and can start a rescan. Assignment deliberately stays on each patch, so the
    only assignment-shaped thing here is the "used by" hint read from `/state`.
*/
export function PluginsPage ({
    inventory,
    assignments = [],
    error = null,
    busy = false,
    initialSearch = "",
    onRescan,
    onReload,
}: PluginsPageProps): ReactElement
{
    const [search, setSearch] = useState (initialSearch);
    const [scanPolls, setScanPolls] = useState (0);

    const scanning = inventory?.scanning === true;
    const all = inventory?.plugins ?? [];
    const shown = filterPlugins (all, search);
    const total = inventory?.total ?? all.length;

    // The POST that starts a scan answers before the scan finishes, so the page
    // re-reads the inventory until the engine reports `scanning: false`. This
    // never runs under `react-dom/server`, where the page is a pure snapshot.
    useEffect (() =>
    {
        if (! scanning || busy || onReload === undefined || scanPolls >= MAX_SCAN_POLLS)
            return undefined;

        const timer = setTimeout (() =>
        {
            setScanPolls (count => count + 1);
            onReload ();
        }, SCAN_POLL_MS);

        return () => { clearTimeout (timer); };
    }, [scanning, busy, scanPolls, onReload]);

    const rescan = (): void =>
    {
        setScanPolls (0);
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
            <header className="plugins__head">
                <div className="plugins__intro">
                    <p className="plugins__eyebrow">Plugins</p>
                    <h2 className="plugins__title">Installed plugins</h2>
                    <p className="plugins__lede">
                        Everything the VST3 scanner found. Search by name, vendor, category
                        or bundle path; assignment stays on each patch.
                    </p>
                </div>

                <div className="plugins__status">
                    <span className="plugins__count" data-testid="plugin-count">
                        {pluginCountLabel (shown.length, total)}
                    </span>
                    <span
                        className={`plugins__scan${scanning ? " plugins__scan--running" : ""}`}
                        data-testid="plugin-scan-state"
                        data-scanning={scanning ? "true" : "false"}
                        data-scanned={inventory?.scanned === true ? "true" : "false"}
                    >
                        {scanStateLabel (inventory)}
                    </span>
                </div>
            </header>

            <div className="plugins__toolbar" role="search">
                <label className="plugins__search" htmlFor="plugin-search-input">
                    <span className="plugins__search-label">Search</span>
                    <input
                        id="plugin-search-input"
                        className="plugins__search-input"
                        type="search"
                        data-testid="plugin-search"
                        placeholder="Name, vendor, category or path"
                        autoComplete="off"
                        spellCheck={false}
                        value={search}
                        onChange={event => { setSearch (event.currentTarget.value); }}
                    />
                </label>

                {search.length > 0
                    ? <button
                          className="plugins__button plugins__button--ghost"
                          type="button"
                          data-testid="plugin-search-clear"
                          onClick={() => { setSearch (""); }}
                      >
                          Clear
                      </button>
                    : null}

                <button
                    className="plugins__button"
                    type="button"
                    data-testid="plugin-rescan"
                    data-busy={busy ? "true" : "false"}
                    data-scanning={scanning ? "true" : "false"}
                    disabled={busy || scanning || onRescan === undefined}
                    onClick={rescan}
                >
                    {scanning ? "Scanning…" : "Rescan"}
                </button>
            </div>

            {error !== null
                ? <div className="plugins__error" data-testid="plugin-error" role="alert">
                      <span>{error}</span>
                      {onReload !== undefined
                          ? <button
                                className="plugins__button plugins__button--small"
                                type="button"
                                data-testid="plugin-retry"
                                onClick={onReload}
                            >
                                Retry
                            </button>
                        : null}
                  </div>
                : null}

            {inventory === null
                ? error === null
                    ? <p className="plugins__empty" data-testid="plugin-loading" aria-busy="true">
                          Reading the VST3 folders…
                      </p>
                    : null
                : shown.length === 0
                    ? <div className="plugins__empty" data-testid="plugin-empty">
                          {search.length > 0
                              ? <span>
                                    No plug-ins match <strong>{search}</strong>.
                                </span>
                              : <span>
                                    No VST3 plug-ins were found. Check that the VST3
                                    folders are installed, then rescan.
                                </span>}
                      </div>
                    : <ol className="plugins__list" data-testid="plugin-list">
                          {shown.map (plugin => (
                              <PluginCard
                                  key={plugin.id}
                                  plugin={plugin}
                                  assignments={assignments}
                              />
                          ))}
                      </ol>}

            <p className="plugins__note" data-testid="plugins-note">
                This page only reads the scan. “Used by” counts patches whose instrument
                source is this plug-in; to change a patch's instrument or insert an
                effect, open that patch on the Instrument page.
            </p>
        </section>
    );
}
