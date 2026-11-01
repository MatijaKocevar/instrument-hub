import type { ControlPatch, ControlPlugin } from "../../api/client";

export function instrumentSlotForRoute(
  route: string,
  requestedSlot: number | null,
  armedSlot: number,
): number | null {
  if (route !== "instrument") return null;

  if (armedSlot >= 0) return armedSlot;

  return requestedSlot;
}

export function followsArmedSlot(armedSlot: number): boolean {
  return armedSlot >= 0;
}

export function patchForInstrumentSlot(
  patches: readonly ControlPatch[],
  slot: number | null,
): ControlPatch | null {
  if (slot === null) return null;

  return patches.find((patch) => patch.slot === slot) ?? null;
}

export function instrumentPlugins(
  plugins: readonly ControlPlugin[],
): ControlPlugin[] {
  return plugins.filter((plugin) => plugin.isInstrument && !plugin.isEffect);
}

export function effectPlugins(
  plugins: readonly ControlPlugin[],
): ControlPlugin[] {
  return plugins.filter((plugin) => plugin.isEffect);
}
