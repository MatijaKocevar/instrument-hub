import type { ControlFxChain, ControlFxInsert } from "../../api/client";

export const CHAIN_INSTRUMENT_LATENCY = 84;
export const COMPRESSOR_LATENCY = 156;

export function fixtureInsert(
  index: number,
  type: ControlFxInsert["type"],
  id: string,
  name: string,
  latencySamples: number,
  parameters: Record<string, number> = {},
): ControlFxInsert {
  return {
    index,
    type,
    kind: type === "vst3" ? "vst3" : "builtin",
    id,
    name,
    bypassed: false,
    wet: 1,
    dry: 0,
    latencySamples,
    parameters,
  };
}

export function recalculateChain(chain: ControlFxChain): void {
  chain.inserts = chain.inserts.map((insert, index) => ({ ...insert, index }));
  chain.fxLatencySamples = chain.inserts.reduce(
    (total, insert) =>
      total + (insert.bypassed || insert.wet === 0 ? 0 : insert.latencySamples),
    0,
  );
  chain.latencySamples =
    chain.instrumentLatencySamples + chain.fxLatencySamples;
  chain.latencyMs = (chain.latencySamples * 1000) / 48000;
}
