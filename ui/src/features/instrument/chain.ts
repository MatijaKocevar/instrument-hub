import type { ControlFxChain, ControlFxInsert } from "../../api/client";

export function moveOrder(length: number, from: number, to: number): number[] {
  if (!Number.isInteger(length) || length <= 0) return [];

  const source = Math.min(length - 1, Math.max(0, from));
  const target = Math.min(length - 1, Math.max(0, to));

  const order = Array.from({ length }, (_, index) => index);
  const [moved] = order.splice(source, 1);

  if (moved === undefined) return order;

  order.splice(target, 0, moved);

  return order;
}

export function withChainLatency(
  chain: ControlFxChain,
  inserts: readonly ControlFxInsert[],
): ControlFxChain {
  const fxLatencySamples = inserts.reduce(
    (total, insert) =>
      total + (insert.bypassed || insert.wet === 0 ? 0 : insert.latencySamples),
    0,
  );

  const latencySamples = chain.instrumentLatencySamples + fxLatencySamples;
  const msPerSample =
    chain.latencySamples > 0 ? chain.latencyMs / chain.latencySamples : 0;

  return {
    ...chain,
    fxLatencySamples,
    latencySamples,
    latencyMs: latencySamples * msPerSample,
    inserts: [...inserts].map((insert, index) => ({ ...insert, index })),
  };
}

export function chainWithoutInsert(
  chain: ControlFxChain,
  index: number,
): ControlFxChain {
  if (index < 0 || index >= chain.inserts.length) return chain;

  return withChainLatency(
    chain,
    chain.inserts.filter((_, position) => position !== index),
  );
}

export function chainWithBypass(
  chain: ControlFxChain,
  index: number,
  bypassed: boolean,
): ControlFxChain {
  if (index < 0 || index >= chain.inserts.length) return chain;

  return withChainLatency(
    chain,
    chain.inserts.map((insert, position) =>
      position === index ? { ...insert, bypassed } : insert,
    ),
  );
}

export function chainWithMove(
  chain: ControlFxChain,
  from: number,
  to: number,
): ControlFxChain {
  if (chain.inserts.length === 0) return chain;

  const order = moveOrder(chain.inserts.length, from, to);
  const reordered: ControlFxInsert[] = [];

  for (const index of order) {
    const insert = chain.inserts[index];

    if (insert !== undefined) reordered.push(insert);
  }

  return withChainLatency(chain, reordered);
}
