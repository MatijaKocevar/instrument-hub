import type { ReactElement } from "react";

interface PluginsToolbarProps
{
    readonly search: string;
    readonly busy: boolean;
    readonly scanning: boolean;
    readonly onSearch: (value: string) => void;
    readonly onClear: () => void;
    readonly onRescan?: (() => void) | undefined;
}

export function PluginsToolbar ({
    search,
    busy,
    scanning,
    onSearch,
    onClear,
    onRescan,
}: PluginsToolbarProps): ReactElement
{
    return (
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
                    onChange={event => { onSearch (event.currentTarget.value); }}
                />
            </label>

            {search.length > 0
                ? <button
                      className="plugins__button plugins__button--ghost"
                      type="button"
                      data-testid="plugin-search-clear"
                      onClick={onClear}
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
                onClick={() => { onRescan?.(); }}
            >
                {scanning ? "Scanning…" : "Rescan"}
            </button>
        </div>
    );
}
