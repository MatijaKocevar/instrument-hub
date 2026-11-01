import { useState, type ReactElement } from "react";

import type { ControlPatch, ControlPatchSourceRequest, ControlPlugin } from "../../api/client";
import { instrumentPlugins } from "./selection";
import {
    sourceRequestForEmpty,
    sourceRequestForInitTone,
    sourceRequestForPlugin,
} from "./source";

interface SourcePickerProps
{
    readonly patch: ControlPatch;
    readonly plugins?: readonly ControlPlugin[] | null;
    readonly pluginError?: string | null;
    readonly sourceBusy?: boolean;
    readonly onSourceChange?: (slot: number, source: ControlPatchSourceRequest) => void;
}

/** The collapsible picker that assigns a built-in or installed instrument source. */
export function SourcePicker ({
    patch,
    plugins,
    pluginError,
    sourceBusy,
    onSourceChange,
}: SourcePickerProps): ReactElement
{
    const [selection, setSelection] = useState ("");
    const assignable = instrumentPlugins (plugins ?? []);

    const choose = (): void =>
    {
        if (selection === "empty")
        {
            onSourceChange?.(patch.slot, sourceRequestForEmpty ());
            return;
        }

        if (selection === "init-tone")
        {
            onSourceChange?.(patch.slot, sourceRequestForInitTone ());
            return;
        }

        const plugin = assignable.find (candidate => candidate.path === selection);

        if (plugin !== undefined)
            onSourceChange?.(patch.slot, sourceRequestForPlugin (plugin));
    };

    return (
        <details className="instrument__details" data-testid="source-change">
            <summary className="instrument__summary">Change source</summary>
            <div className="instrument__picker">
                <label className="instrument__field" htmlFor="instrument-source-select">
                    <span className="instrument__field-label">Installed source</span>
                    <select
                        id="instrument-source-select"
                        className="instrument__select"
                        data-testid="source-select"
                        value={selection}
                        onChange={event => { setSelection (event.currentTarget.value); }}
                    >
                        <option value="">Choose a source…</option>
                        <option value="empty">Unassigned (silent)</option>
                        <option value="init-tone">Built-in tone</option>
                        {assignable.map (plugin => (
                            <option key={plugin.id} value={plugin.path}>
                                {plugin.name} · {plugin.vendor}
                            </option>
                        ))}
                    </select>
                </label>

                <button
                    className="instrument__button"
                    type="button"
                    data-testid="source-apply"
                    data-busy={sourceBusy === true ? "true" : "false"}
                    disabled={selection.length === 0 || sourceBusy === true || onSourceChange === undefined}
                    onClick={choose}
                >
                    Apply source
                </button>

                {assignable.length === 0
                    ? <p className="instrument__field-hint" data-testid="source-empty">
                          {pluginError ?? "No instrument plug-ins are installed."}
                      </p>
                    : null}
            </div>
        </details>
    );
}
