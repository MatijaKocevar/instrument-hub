import type {
  StateSocketEvent,
  StateSocketEventLike,
  StateSocketLike,
} from "../../state/stateSocket";

export class FakeStateSocket implements StateSocketLike {
  readonly listeners = new Map<
    StateSocketEvent,
    ((event: StateSocketEventLike) => void)[]
  >();
  closed = false;

  addEventListener(
    type: StateSocketEvent,
    listener: (event: StateSocketEventLike) => void,
  ): void {
    const existing = this.listeners.get(type) ?? [];
    existing.push(listener);
    this.listeners.set(type, existing);
  }

  close(): void {
    this.closed = true;
    this.emit("close", {});
  }

  emit(type: StateSocketEvent, event: StateSocketEventLike): void {
    for (const listener of this.listeners.get(type) ?? []) listener(event);
  }

  push(frame: unknown): void {
    this.emit("message", { data: JSON.stringify(frame) });
  }
}
