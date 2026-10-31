import type {
    ControlPatch,
    ControlPlugin,
    ControlState,
} from "../../api/client";

/**
    A patch that currently uses an installed plug-in as its instrument source.

    Assignment lives on the patch (see the Instrument page), never on the
    inventory. The Plugins page only reads these back to answer "where is this
    plug-in in use?".
*/
export interface PluginAssignment
{
    /** Engine slot of the patch. */
    readonly slot: number;
    readonly patchName: string;
    readonly familyName: string;
    /** Stable plug-in identity recorded on the patch, or "". */
    readonly pluginId: string;
    /** Display name recorded on the patch, used when the id is missing. */
    readonly pluginName: string;
}

/** Every plugin-backed patch in the set, in family/patch order. */
export function pluginAssignments (patches: readonly ControlPatch[]): PluginAssignment[]
{
    const assignments: PluginAssignment[] = [];

    for (const patch of patches)
    {
        if (patch.instrument.kind !== "plugin")
            continue;

        assignments.push ({
            slot: patch.slot,
            patchName: patch.name,
            familyName: patch.familyName,
            pluginId: patch.instrument.pluginId ?? "",
            pluginName: patch.instrument.pluginName ?? patch.instrument.name,
        });
    }

    return assignments;
}

/** The same list, straight from a `/state` family tree. */
export function pluginAssignmentsFromState (state: ControlState | null): PluginAssignment[]
{
    if (state === null)
        return [];

    return pluginAssignments (state.families.flatMap (family => family.patches));
}

/**
    True when `assignment` refers to `plugin`.

    The stable id wins when both sides carry one; the display name is only a
    fallback for older set files that predate `pluginId`.
*/
function assignmentMatches (assignment: PluginAssignment, plugin: ControlPlugin): boolean
{
    if (assignment.pluginId.length > 0 && plugin.id.length > 0)
        return assignment.pluginId === plugin.id;

    return assignment.pluginName.length > 0 && assignment.pluginName === plugin.name;
}

/** The patches using `plugin`, in set order. */
export function usedByPlugin (assignments: readonly PluginAssignment[],
                              plugin: ControlPlugin): PluginAssignment[]
{
    return assignments.filter (assignment => assignmentMatches (assignment, plugin));
}

/** True when any patch uses the plug-in. */
export function isPluginInUse (assignments: readonly PluginAssignment[],
                               plugin: ControlPlugin): boolean
{
    return assignments.some (assignment => assignmentMatches (assignment, plugin));
}

/**
    Case-insensitive search over everything the scanner published.

    Every whitespace-separated term must match at least one field, so
    "arturia instrument" narrows instead of widening. An empty query matches
    everything.
*/
export function filterPlugins (plugins: readonly ControlPlugin[],
                               query: string): ControlPlugin[]
{
    const terms = query.trim ().toLowerCase ().split (/\s+/).filter (term => term.length > 0);

    if (terms.length === 0)
        return [...plugins];

    return plugins.filter (plugin =>
    {
        const haystack = [
            plugin.name,
            plugin.description,
            plugin.vendor,
            plugin.version,
            plugin.category,
            plugin.id,
            plugin.format,
            plugin.path,
        ].join (" ").toLowerCase ();

        return terms.every (term => haystack.includes (term));
    });
}

/** Human label for the plug-in's declared role. */
export function pluginKind (plugin: ControlPlugin): string
{
    if (plugin.isInstrument && plugin.isEffect)
        return "Instrument + FX";

    if (plugin.isInstrument)
        return "Instrument";

    if (plugin.isEffect)
        return "Effect";

    return "Unknown";
}

/** "3 of 12 plug-ins" with the right singular form. */
export function pluginCountLabel (shown: number, total: number): string
{
    const noun = total === 1 ? "plug-in" : "plug-ins";

    return shown === total
        ? `${String (total)} ${noun}`
        : `${String (shown)} of ${String (total)} ${noun}`;
}

/**
    Compact timestamp for the inventory header.

    `lastScanMs` is Unix epoch milliseconds reported by the engine. A missing
    or implausible value renders as "never", never as the 1970 epoch.
*/
export function formatScanTime (lastScanMs: number): string
{
    if (! Number.isFinite (lastScanMs) || lastScanMs <= 0)
        return "never";

    const date = new Date (lastScanMs);

    if (Number.isNaN (date.getTime ()))
        return "never";

    return date.toLocaleString ("en-GB", {
        day: "2-digit",
        month: "short",
        hour: "2-digit",
        minute: "2-digit",
    });
}
