import { describeMidiData } from "./webMidi";

export interface MidiTelemetrySnapshot {
  readonly count: number;
  readonly lastEvent: string;
}

const INITIAL: MidiTelemetrySnapshot = { count: 0, lastEvent: "—" };

export class MidiTelemetry {
  private snapshot: MidiTelemetrySnapshot = INITIAL;
  private readonly listeners = new Set<() => void>();

  getSnapshot = (): MidiTelemetrySnapshot => this.snapshot;

  subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);

    return () => {
      this.listeners.delete(listener);
    };
  };

  record(data: Uint8Array): void {
    this.snapshot = {
      count: this.snapshot.count + 1,
      lastEvent: describeMidiData(data),
    };

    for (const listener of this.listeners) listener();
  }

  reset(lastEvent: string): void {
    this.snapshot = { count: 0, lastEvent };

    for (const listener of this.listeners) listener();
  }
}
