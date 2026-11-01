import type { ControlFxInsertRequest, ControlPlugin } from "../../api/client";

/** The built-in effect types `POST /patches/{slot}/fx` accepts. */
export const BUILTIN_INSERT_TYPES = ["eq", "compressor", "delay", "reverb"] as const;

export type BuiltinInsertType = (typeof BUILTIN_INSERT_TYPES)[number];

/** Display names for the insert types. */
const INSERT_LABELS: Record<string, string> = {
    eq: "EQ",
    compressor: "Compressor",
    delay: "Delay",
    reverb: "Reverb",
    vst3: "VST3",
};

/** Human label for an insert type. */
export function insertLabel (type: string): string
{
    return INSERT_LABELS[type] ?? type;
}

/** The insert request that loads an installed effect plugin. */
export function insertRequestForPlugin (plugin: ControlPlugin): ControlFxInsertRequest
{
    return {
        type: "vst3",
        path: plugin.path,
        id: plugin.id,
        name: plugin.name,
        bypassed: false,
        wet: 1,
        position: -1,
    };
}

/** The insert request for a built-in effect. */
export function insertRequestForBuiltin (type: BuiltinInsertType): ControlFxInsertRequest
{
    return { type, bypassed: false, wet: 1, position: -1 };
}
