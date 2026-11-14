import type { ControlFxInsertRequest, ControlPlugin } from "../../api/client";

export const BUILTIN_INSERT_TYPES = [
  "eq",
  "compressor",
  "delay",
  "reverb",
] as const;

export type BuiltinInsertType = (typeof BUILTIN_INSERT_TYPES)[number];

const INSERT_LABELS: Record<string, string> = {
  eq: "EQ",
  compressor: "Compressor",
  delay: "Delay",
  reverb: "Reverb",
  drive: "Drive",
  tuner: "Tuner",
  vst3: "VST3",
};

export function uniqueInsertId(
  base: string,
  existing: readonly string[],
): string {
  if (!existing.includes(base)) return base;

  let suffix = 2;

  while (existing.includes(`${base}-${String(suffix)}`)) suffix += 1;

  return `${base}-${String(suffix)}`;
}

export function insertLabel(type: string): string {
  return INSERT_LABELS[type] ?? type;
}

export function insertRequestForPlugin(
  plugin: ControlPlugin,
): ControlFxInsertRequest {
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

export function insertRequestForBuiltin(
  type: BuiltinInsertType,
): ControlFxInsertRequest {
  return { type, bypassed: false, wet: 1, position: -1 };
}
