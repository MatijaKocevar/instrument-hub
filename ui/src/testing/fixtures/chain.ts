// Insert-chain fixtures served by the snapshot fixture server.

import type { ControlFxChain, ControlFxInsert } from "../../api/client";

/**
    Insert-chain fixture for the instrument page.

    Slot 3 is armed in `/state`, so its chain is the one the page renders: an
    EQ with no latency and a compressor with 156 samples, on top of 84 samples
    of instrument latency, for the 240 samples / 5 ms the patch already
    publishes.
*/
export const CHAIN_INSTRUMENT_LATENCY = 84;
export const COMPRESSOR_LATENCY = 156;

export function fixtureInsert (index: number,
                               type: ControlFxInsert["type"],
                               id: string,
                               name: string,
                               latencySamples: number,
                               parameters: Record<string, number> = {}): ControlFxInsert
{
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

/** Recompute indices and the latency budget of a fixture chain. */
export function recalculateChain (chain: ControlFxChain): void
{
    chain.inserts = chain.inserts.map ((insert, index) => ({ ...insert, index }));
    chain.fxLatencySamples = chain.inserts.reduce (
        (total, insert) => total + (insert.bypassed || insert.wet === 0 ? 0 : insert.latencySamples),
        0);
    chain.latencySamples = chain.instrumentLatencySamples + chain.fxLatencySamples;
    chain.latencyMs = (chain.latencySamples * 1000) / 48000;
}
