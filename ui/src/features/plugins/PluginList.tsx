import type { ReactElement } from "react";

import type { ControlPlugin } from "../../api/client";
import type { PluginAssignment } from "./model";
import { PluginCard } from "./PluginCard";

interface PluginListProps
{
    /** True until the first inventory response arrives. */
    readonly loading: boolean;
    /** Plug-ins left after the search filter. */
    readonly plugins: readonly ControlPlugin[];
    /** Current query, repeated by the empty state. */
    readonly search: string;
    /** Every plugin-backed patch in the set. */
    readonly assignments: readonly PluginAssignment[];
}

/** The inventory list, with its loading and empty states. */
export function PluginList ({
    loading,
    plugins,
    search,
    assignments,
}: PluginListProps): ReactElement
{
    if (loading)
    {
        return (
            <p className="plugins__empty" data-testid="plugin-loading" aria-busy="true">
                Reading the VST3 folders…
            </p>
        );
    }

    if (plugins.length === 0)
    {
        return (
            <div className="plugins__empty" data-testid="plugin-empty">
                {search.length > 0
                    ? <span>
                          No plug-ins match <strong>{search}</strong>.
                      </span>
                    : <span>
                          No VST3 plug-ins were found. Check that the VST3
                          folders are installed, then rescan.
                      </span>}
            </div>
        );
    }

    return (
        <ol className="plugins__list" data-testid="plugin-list">
            {plugins.map (plugin => (
                <PluginCard
                    key={plugin.id}
                    plugin={plugin}
                    assignments={assignments}
                />
            ))}
        </ol>
    );
}
