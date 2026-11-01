/** Screen-wake-lock life-cycle. */
export type WakeLockStatus = "idle" | "requesting" | "active" | "unavailable" | "denied";

/** The slice of `WakeLockSentinel` the controller needs. */
export interface WakeLockSentinelLike
{
    readonly released: boolean;
    release (): Promise<void>;
    addEventListener? (type: "release", listener: () => void): void;
}

/** The slice of `Navigator` the controller needs; lets tests inject a fake. */
export interface WakeLockCapableNavigator
{
    readonly wakeLock?: {
        request (type: "screen"): Promise<WakeLockSentinelLike>;
    };
}

export interface WakeLockControllerOptions
{
    navigator?: WakeLockCapableNavigator | null;
    /** Called on every status change, so a hook can re-render. */
    onStatus?: (status: WakeLockStatus) => void;
}
