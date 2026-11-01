import type {
    ControlPatch,
    ControlPlugin,
    ControlState,
} from "../../api/client";

export interface PluginAssignment
{
    readonly slot: number;
    readonly patchName: string;
    readonly familyName: string;
    readonly pluginId: string;
    readonly pluginName: string;
}

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

export function pluginAssignmentsFromState (state: ControlState | null): PluginAssignment[]
{
    if (state === null)
        return [];

    return pluginAssignments (state.families.flatMap (family => family.patches));
}

function assignmentMatches (assignment: PluginAssignment, plugin: ControlPlugin): boolean
{
    if (assignment.pluginId.length > 0 && plugin.id.length > 0)
        return assignment.pluginId === plugin.id;

    return assignment.pluginName.length > 0 && assignment.pluginName === plugin.name;
}

export function usedByPlugin (assignments: readonly PluginAssignment[],
                              plugin: ControlPlugin): PluginAssignment[]
{
    return assignments.filter (assignment => assignmentMatches (assignment, plugin));
}

export function isPluginInUse (assignments: readonly PluginAssignment[],
                               plugin: ControlPlugin): boolean
{
    return assignments.some (assignment => assignmentMatches (assignment, plugin));
}

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

export function pluginCountLabel (shown: number, total: number): string
{
    const noun = total === 1 ? "plug-in" : "plug-ins";

    return shown === total
        ? `${String (total)} ${noun}`
        : `${String (shown)} of ${String (total)} ${noun}`;
}

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
