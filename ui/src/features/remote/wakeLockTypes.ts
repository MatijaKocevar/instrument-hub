export type WakeLockStatus =
  "idle" | "requesting" | "active" | "unavailable" | "denied";

export interface WakeLockSentinelLike {
  readonly released: boolean;
  release(): Promise<void>;
  addEventListener?(type: "release", listener: () => void): void;
}

export interface WakeLockCapableNavigator {
  readonly wakeLock?: {
    request(type: "screen"): Promise<WakeLockSentinelLike>;
  };
}

export interface WakeLockControllerOptions {
  navigator?: WakeLockCapableNavigator | null;
  onStatus?: (status: WakeLockStatus) => void;
}
