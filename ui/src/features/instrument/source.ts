import type { ControlPatch, ControlPatchSourceRequest, ControlPlugin } from "../../api/client";

export function isPluginSource (patch: ControlPatch | null): boolean
{
    return patch?.instrument.kind === "plugin";
}

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

export function sourceRequestForInitTone (frequency?: number): ControlPatchSourceRequest
{
    return {
        kind: "init-tone",
        ...(frequency !== undefined ? { frequency } : {}),
    };
}

export function sourceRequestForEmpty (): ControlPatchSourceRequest
{
    return { kind: "empty" };
}
