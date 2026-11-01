import type {
    WakeLockCapableNavigator,
    WakeLockControllerOptions,
    WakeLockSentinelLike,
    WakeLockStatus,
} from "./wakeLockTypes";

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

    start (): Promise<WakeLockStatus>
    {
        this.started = true;

        return this.acquire ();
    }

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
            }
        }
    }

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
