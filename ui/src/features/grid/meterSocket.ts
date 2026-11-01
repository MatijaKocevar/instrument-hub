export type MeterSocketEvent = "open" | "close" | "error" | "message";

export interface MeterSocketEventLike {
  readonly data?: unknown;
}

export interface MeterSocketLike {
  addEventListener(
    type: MeterSocketEvent,
    listener: (event: MeterSocketEventLike) => void,
  ): void;
  close(): void;
}

export type MeterSocketFactory = (url: string) => MeterSocketLike;

export function defaultSocketFactory(url: string): MeterSocketLike {
  const Constructor = (
    globalThis as {
      WebSocket?: new (url: string) => unknown;
    }
  ).WebSocket;

  if (Constructor === undefined)
    throw new Error("WebSocket is not available in this environment");

  return new Constructor(url) as MeterSocketLike;
}
