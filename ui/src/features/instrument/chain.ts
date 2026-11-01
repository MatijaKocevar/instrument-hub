import type { ControlFxChain, ControlFxInsert } from "../../api/client";

/** `order` that moves `from` to `to`, for `POST /patches/{slot}/fx/reorder`. */
export function moveOrder (length: number, from: number, to: number): number[]
{
    if (! Number.isInteger (length) || length <= 0)
        return [];

    const source = Math.min (length - 1, Math.max (0, from));
    const target = Math.min (length - 1, Math.max (0, to));

    const order = Array.from ({ length }, (_, index) => index);
    const [moved] = order.splice (source, 1);

    if (moved === undefined)
        return order;

    order.splice (target, 0, moved);

    return order;
}

/**
    Recompute a chain's latency budget after an optimistic edit.

    A bypassed insert (or one at `wet: 0`) contributes no latency, exactly like
    the engine. The millisecond conversion reuses the ratio in the chain the
    server last reported, so the readout stays consistent across a sample-rate
    change the UI has not seen.
*/
export function withChainLatency (chain: ControlFxChain,
                                  inserts: readonly ControlFxInsert[]): ControlFxChain
{
    const fxLatencySamples = inserts.reduce (
        (total, insert) => total + (insert.bypassed || insert.wet === 0 ? 0 : insert.latencySamples),
        0);

    const latencySamples = chain.instrumentLatencySamples + fxLatencySamples;
    const msPerSample = chain.latencySamples > 0
        ? chain.latencyMs / chain.latencySamples
        : 0;

    return {
        ...chain,
        fxLatencySamples,
        latencySamples,
        latencyMs: latencySamples * msPerSample,
        inserts: [...inserts].map ((insert, index) => ({ ...insert, index })),
    };
}

/** The chain as it will look after one insert is removed. */
export function chainWithoutInsert (chain: ControlFxChain, index: number): ControlFxChain
{
    if (index < 0 || index >= chain.inserts.length)
        return chain;

    return withChainLatency (chain, chain.inserts.filter ((_, position) => position !== index));
}

/** The chain as it will look after one insert is bypassed or re-enabled. */
export function chainWithBypass (chain: ControlFxChain, index: number, bypassed: boolean): ControlFxChain
{
    if (index < 0 || index >= chain.inserts.length)
        return chain;

    return withChainLatency (chain, chain.inserts.map ((insert, position) =>
        position === index ? { ...insert, bypassed } : insert));
}

/** The chain as it will look after one insert moves to `to`. */
export function chainWithMove (chain: ControlFxChain, from: number, to: number): ControlFxChain
{
    if (chain.inserts.length === 0)
        return chain;

    const order = moveOrder (chain.inserts.length, from, to);
    const reordered: ControlFxInsert[] = [];

    for (const index of order)
    {
        const insert = chain.inserts[index];

        if (insert !== undefined)
            reordered.push (insert);
    }

    return withChainLatency (chain, reordered);
}
