export type StateSocketEvent = "open" | "close" | "error" | "message";

export interface StateSocketEventLike {
  readonly data?: unknown;
}

export interface StateSocketLike {
  addEventListener(
    type: StateSocketEvent,
    listener: (event: StateSocketEventLike) => void,
  ): void;
  close(): void;
}

export type StateSocketFactory = (url: string) => StateSocketLike;

export function defaultStateSocketFactory(url: string): StateSocketLike {
  const Constructor = (
    globalThis as {
      WebSocket?: new (url: string) => unknown;
    }
  ).WebSocket;

  if (Constructor === undefined)
    throw new Error("WebSocket is not available in this environment");

  return new Constructor(url) as StateSocketLike;
}
