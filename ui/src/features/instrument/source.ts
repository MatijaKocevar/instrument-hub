import type { ControlPatch, ControlPatchSourceRequest, ControlPlugin } from "../../api/client";

/** True when the patch's source is a hosted plugin (the editor has a window). */
export function isPluginSource (patch: ControlPatch | null): boolean
{
    return patch?.instrument.kind === "plugin";
}

/** Human label for the declared source kind. */
export function sourceKindLabel (kind: string): string
{
    switch (kind)
    {
        case "empty": return "Unassigned";
        case "plugin": return "VST3 plug-in";
        case "sampler": return "Sampler kit";
        case "audio-input": return "Live audio input";
        case "init-tone": return "Built-in tone";
        default: return "Unassigned";
    }
}

/** The source request that assigns an installed plugin to a patch. */
export function sourceRequestForPlugin (plugin: ControlPlugin): ControlPatchSourceRequest
{
    return {
        kind: "plugin",
        name: plugin.name,
        path: plugin.path,
        pluginFormat: plugin.format,
        pluginId: plugin.id,
        pluginName: plugin.name,
        pluginManufacturer: plugin.vendor,
        pluginVersion: plugin.version,
    };
}

/** The source request for the built-in initialisation tone. */
export function sourceRequestForInitTone (frequency?: number): ControlPatchSourceRequest
{
    return {
        kind: "init-tone",
        ...(frequency !== undefined ? { frequency } : {}),
    };
}

/** The source request that leaves a patch unassigned (silent). */
export function sourceRequestForEmpty (): ControlPatchSourceRequest
{
    return { kind: "empty" };
}
