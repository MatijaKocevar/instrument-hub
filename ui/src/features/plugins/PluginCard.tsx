import type { ReactElement } from "react";

import type { ControlPlugin } from "../../api/client";
import { pluginKind, usedByPlugin, type PluginAssignment } from "./model";
import { PluginUsedBy } from "./PluginUsedBy";

interface PluginCardProps
{
    readonly plugin: ControlPlugin;
    /** Every plugin-backed patch in the set. */
    readonly assignments: readonly PluginAssignment[];
}

/** One scanned plug-in: identity, metadata, bundle path and its used-by hint. */
export function PluginCard ({ plugin, assignments }: PluginCardProps): ReactElement
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

            <PluginUsedBy usage={usage} />
        </li>
    );
}
