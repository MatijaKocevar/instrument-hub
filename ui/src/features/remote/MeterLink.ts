import { MeterStore } from "../grid";

export interface RemoteLinkSnapshot {
  readonly connected: boolean;
  readonly everConnected: boolean;
  readonly lastLiveAt: number | null;
  readonly failures: number;
}

export const INITIAL_LINK: RemoteLinkSnapshot = {
  connected: false,
  everConnected: false,
  lastLiveAt: null,
  failures: 0,
};

export interface MeterLinkOptions {
  now?: () => number;
}

function sameLink(a: RemoteLinkSnapshot, b: RemoteLinkSnapshot): boolean {
  return (
    a.connected === b.connected &&
    a.everConnected === b.everConnected &&
    a.lastLiveAt === b.lastLiveAt &&
    a.failures === b.failures
  );
}

export function linkPhase(link: RemoteLinkSnapshot | null): string {
  if (link === null) return "unknown";

  if (link.connected) return "live";

  return link.everConnected ? "lost" : "connecting";
}

export class MeterLink {
  private readonly meters: MeterStore;
  private readonly now: () => number;
  private readonly listeners = new Set<() => void>();

  private snapshot: RemoteLinkSnapshot = INITIAL_LINK;
  private unsubscribe: (() => void) | null = null;
  private lastLiveAt: number | null = null;
  private wasConnected = false;

  constructor(meters: MeterStore, options: MeterLinkOptions = {}) {
    this.meters = meters;
    this.now = options.now ?? (() => Date.now());
  }

  getSnapshot = (): RemoteLinkSnapshot => this.snapshot;

  subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);

    return () => {
      this.listeners.delete(listener);
    };
  };

  start(): void {
    if (this.unsubscribe !== null) return;

    this.unsubscribe = this.meters.subscribe(this.sync);
    this.sync();
    this.meters.start();
  }

  stop(): void {
    this.unsubscribe?.();
    this.unsubscribe = null;

    this.meters.stop();

    this.lastLiveAt = null;
    this.wasConnected = false;
    this.publish(INITIAL_LINK);
  }

  private sync = (): void => {
    const meters = this.meters.getSnapshot();

    if (meters.connected) this.lastLiveAt = this.now();

    const failures = meters.connected
      ? 0
      : this.wasConnected
        ? this.snapshot.failures + 1
        : this.snapshot.failures;

    this.wasConnected = meters.connected;

    this.publish({
      connected: meters.connected,
      everConnected: this.snapshot.everConnected || meters.connected,
      lastLiveAt: this.lastLiveAt,
      failures,
    });
  };

  private publish(next: RemoteLinkSnapshot): void {
    if (sameLink(this.snapshot, next)) return;

    this.snapshot = next;

    for (const listener of this.listeners) listener();
  }
}
