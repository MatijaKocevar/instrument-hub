import { useState, type ReactElement } from "react";

import type { ControlFxInsertRequest, ControlPatch, ControlPlugin } from "../../api/client";
import {
    BUILTIN_INSERT_TYPES,
    effectPlugins,
    insertLabel,
    insertRequestForBuiltin,
    insertRequestForPlugin,
    type BuiltinInsertType,
} from "./model";

interface InsertAddProps
{
    readonly patch: ControlPatch;
    readonly plugins?: readonly ControlPlugin[] | null;
    readonly chainBusy?: boolean;
    readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
}

/** The chain's add control: built-in insert types and installed effects. */
export function InsertAdd ({ patch, plugins, chainBusy, onAddInsert }: InsertAddProps): ReactElement
{
    const [selection, setSelection] = useState<string> (`builtin:${BUILTIN_INSERT_TYPES[0]}`);
    const effects = effectPlugins (plugins ?? []);

    const add = (): void =>
    {
        if (selection.startsWith ("builtin:"))
        {
            const type = selection.slice ("builtin:".length) as BuiltinInsertType;

            if ((BUILTIN_INSERT_TYPES as readonly string[]).includes (type))
                onAddInsert?.(patch.slot, insertRequestForBuiltin (type));

            return;
        }

        if (selection.startsWith ("plugin:"))
        {
            const plugin = effects.find (candidate => candidate.id === selection.slice ("plugin:".length));

            if (plugin !== undefined)
                onAddInsert?.(patch.slot, insertRequestForPlugin (plugin));
        }
    };

    return (
        <div className="instrument__add" data-testid="fx-add">
            <label className="instrument__field" htmlFor="instrument-insert-select">
                <span className="instrument__field-label">Add insert</span>
                <select
                    id="instrument-insert-select"
                    className="instrument__select"
                    data-testid="fx-add-select"
                    value={selection}
                    onChange={event => { setSelection (event.currentTarget.value); }}
                >
                    {BUILTIN_INSERT_TYPES.map (type => (
                        <option key={type} value={`builtin:${type}`}>
                            {insertLabel (type)}
                        </option>
                    ))}
                    {effects.map (plugin => (
                        <option key={plugin.id} value={`plugin:${plugin.id}`}>
                            {plugin.name} · {plugin.vendor}
                        </option>
                    ))}
                </select>
            </label>

            <button
                className="instrument__button"
                type="button"
                data-testid="fx-add-button"
                disabled={chainBusy === true || onAddInsert === undefined}
                onClick={add}
            >
                Add insert
            </button>
        </div>
    );
}
