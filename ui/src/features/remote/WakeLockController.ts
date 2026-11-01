import type {
    WakeLockCapableNavigator,
    WakeLockControllerOptions,
    WakeLockSentinelLike,
    WakeLockStatus,
} from "./wakeLockTypes";

/**
    Keep the tablet screen awake while the stage remote is open.

    The Screen Wake Lock API is not available everywhere (older Android
    WebViews, non-secure origins) and the browser drops the lock whenever the
    page is hidden. This controller therefore treats the lock as best-effort:
    unsupported or rejected requests are reported, never thrown, and the lock
    is re-requested every time the page becomes visible again.

    It never touches the audio thread; it is pure browser policy.
*/
export class WakeLockController
{
    private readonly navigator: WakeLockCapableNavigator | null;
    private readonly onStatus: ((status: WakeLockStatus) => void) | undefined;

    private sentinel: WakeLockSentinelLike | null = null;
    private statusValue: WakeLockStatus = "idle";
    private started = false;
    private requesting: Promise<WakeLockStatus> | null = null;

    constructor (options: WakeLockControllerOptions = {})
    {
        this.navigator = options.navigator ?? null;
        this.onStatus = options.onStatus;
    }

    get status (): WakeLockStatus
    {
        return this.statusValue;
    }

    /** Ask for the lock; a second call while active is a no-op. */
    start (): Promise<WakeLockStatus>
    {
        this.started = true;

        return this.acquire ();
    }

    /**
        Ask the browser for a screen lock.

        Resolves with the resulting status instead of rejecting: a stage UI
        must not break because a privacy setting said no.
    */
    acquire (): Promise<WakeLockStatus>
    {
        if (! this.started)
            return Promise.resolve (this.statusValue);

        const api = this.navigator?.wakeLock;

        if (api === undefined)
        {
            this.set ("unavailable");
            return Promise.resolve ("unavailable");
        }

        if (this.sentinel !== null && ! this.sentinel.released)
        {
            this.set ("active");
            return Promise.resolve ("active");
        }

        if (this.requesting !== null)
            return this.requesting;

        this.set ("requesting");

        this.requesting = Promise.resolve ()
            .then (() => api.request ("screen"))
            .then (sentinel =>
            {
                this.sentinel = sentinel;

                sentinel.addEventListener?. ("release", () =>
                {
                    if (this.sentinel === sentinel)
                        this.sentinel = null;

                    if (this.started)
                        this.set ("idle");
                });

                this.set ("active");
                return "active" as const;
            })
            .catch (() =>
            {
                this.sentinel = null;
                this.set ("denied");
                return "denied" as const;
            })
            .finally (() =>
            {
                this.requesting = null;
            });

        return this.requesting;
    }

    /** Drop the lock; used when the route unmounts. */
    async release (): Promise<void>
    {
        this.started = false;

        const sentinel = this.sentinel;
        this.sentinel = null;
        this.set ("idle");

        if (sentinel !== null && ! sentinel.released)
        {
            try
            {
                await sentinel.release ();
            }
            catch
            {
                // The browser may have released it already.
            }
        }
    }

    /**
        React to `visibilitychange`.

        Hidden pages lose the lock automatically, so the controller forgets the
        old sentinel; returning to the page re-requests a fresh one.
    */
    visibilityChanged (visible = true): Promise<WakeLockStatus>
    {
        if (! this.started)
            return Promise.resolve (this.statusValue);

        if (! visible)
        {
            const sentinel = this.sentinel;
            this.sentinel = null;
            this.set ("idle");

            if (sentinel !== null && ! sentinel.released)
                void sentinel.release ().catch (() => {});

            return Promise.resolve ("idle");
        }

        return this.acquire ();
    }

    private set (status: WakeLockStatus): void
    {
        if (this.statusValue === status)
            return;

        this.statusValue = status;
        this.onStatus?.(status);
    }
}
