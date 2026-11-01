/** Timing knobs for the poll loop; the store resolves their defaults. */
export interface PollLoopOptions
{
    pollIntervalMs: number;
    retryBaseMs: number;
    retryMaxMs: number;
}

/**
    Runs one refresh at a time and schedules the next poll or retry.

    Concurrent `refresh()` callers share the request in flight, so a slow
    engine cannot stack requests. Failures back off exponentially from
    `retryBaseMs` up to `retryMaxMs`; a success returns to `pollIntervalMs`.
*/
export class PollLoop
{
    private timer: ReturnType<typeof setTimeout> | null = null;
    private inFlight: Promise<boolean> | null = null;
    private running = false;

    constructor (private readonly options: PollLoopOptions,
                 private readonly load: () => Promise<boolean>,
                 private readonly failedAttempts: () => number) {}

    /** Fetch once, sharing the request with any caller already waiting. */
    refresh = (): Promise<boolean> =>
    {
        if (this.inFlight !== null)
            return this.inFlight;

        this.inFlight = this.load().finally (() =>
        {
            this.inFlight = null;
        });

        return this.inFlight;
    };

    /** Begin polling. Safe to call more than once. */
    start (): void
    {
        if (this.running)
            return;

        this.running = true;
        this.schedule (0);
    }

    /** Stop polling and drop any pending timer. */
    stop (): void
    {
        this.running = false;

        if (this.timer !== null)
        {
            clearTimeout (this.timer);
            this.timer = null;
        }
    }

    /** Delay before the next attempt after `failedAttempts` consecutive misses. */
    retryDelayMs (): number
    {
        const exponent = Math.max (0, this.failedAttempts() - 1);
        const delay = this.options.retryBaseMs * 2 ** exponent;

        return Math.min (this.options.retryMaxMs, delay);
    }

    private schedule (delayMs: number): void
    {
        if (! this.running)
            return;

        this.timer = setTimeout (() =>
        {
            this.timer = null;
            void this.tick();
        }, delayMs);
    }

    private async tick (): Promise<void>
    {
        const ok = await this.refresh();

        if (! this.running)
            return;

        this.schedule (ok ? this.options.pollIntervalMs : this.retryDelayMs());
    }
}
