import type { ControlApi, ControlState } from "../api/client";

/** Connection lifecycle for the polling loop. */
export type ConnectionPhase = "idle" | "connecting" | "ready" | "offline";

/** Immutable view of the store, read through `useSyncExternalStore`. */
export interface AppSnapshot
{
    phase: ConnectionPhase;
    /** Last successful `GET /state`, kept during reconnects. */
    state: ControlState | null;
    /** Message from the most recent failure, cleared on success. */
    error: string | null;
    /** Consecutive failures; drives the exponential backoff. */
    failedAttempts: number;
    /** `Date.now()` of the last successful snapshot, `null` before the first. */
    updatedAt: number | null;
}

export interface AppStoreOptions
{
    /** Delay between successful polls. */
    pollIntervalMs?: number;
    /** First retry delay after a failure; doubles per failure. */
    retryBaseMs?: number;
    /** Upper bound for the retry delay. */
    retryMaxMs?: number;
    /** Injectable clock, used by deterministic tests. */
    now?: () => number;
}

const DEFAULT_OPTIONS: Required<AppStoreOptions> = {
    pollIntervalMs: 2000,
    retryBaseMs: 250,
    retryMaxMs: 5000,
    now: () => Date.now(),
};

function messageOf (error: unknown): string
{
    if (error instanceof Error && error.message.length > 0)
        return error.message;

    if (typeof error === "string" && error.length > 0)
        return error;

    return "the control API is unreachable";
}

/**
    Return `state` with `slot` armed.

    Used for the optimistic highlight: the grid reads `armedSlot`, so patching
    this in the same tick as the click makes the highlight move on the next
    render instead of after the next 2 s poll.
*/
function withArmedSlot (state: ControlState, slot: number): ControlState
{
    return {
        ...state,
        armedSlot: slot,
        slots: state.slots.map (candidate => ({
            ...candidate,
            armed: candidate.index === slot,
        })),
    };
}

/**
    Holds the control state and keeps it fresh.

    A single polling loop serialises requests: `refresh()` de-duplicates while a
    request is in flight, so a slow engine can never stack requests. Failures
    back off exponentially (250 ms doubling to 5 s) and the last good state
    stays visible, which keeps the UI usable across an engine restart.

    Nothing here touches the audio thread — this is pure UI state.
*/
export class AppStore
{
    private readonly api: ControlApi;
    private readonly options: Required<AppStoreOptions>;
    private readonly listeners = new Set<() => void>();

    private snapshot: AppSnapshot = {
        phase: "idle",
        state: null,
        error: null,
        failedAttempts: 0,
        updatedAt: null,
    };

    private timer: ReturnType<typeof setTimeout> | null = null;
    private inFlight: Promise<boolean> | null = null;
    private running = false;

    /** Sequence of arm requests; a stale response must not win. */
    private armSequence = 0;
    /** Number of arm requests in flight, used to keep the highlight during polls. */
    private armInFlight = 0;

    constructor (api: ControlApi, options: AppStoreOptions = {})
    {
        this.api = api;
        this.options = { ...DEFAULT_OPTIONS, ...options };
    }

    getSnapshot = (): AppSnapshot => this.snapshot;

    /** Origin of the control server, shown in the connection strip. */
    get baseUrl (): string
    {
        return this.api.baseUrl;
    }

    subscribe = (listener: () => void): (() => void) =>
    {
        this.listeners.add (listener);

        return () =>
        {
            this.listeners.delete (listener);
        };
    };

    /**
        Fetch the state once.

        Resolves `true` on success and `false` on failure; concurrent callers
        share one request.
    */
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
        const exponent = Math.max (0, this.snapshot.failedAttempts - 1);
        const delay = this.options.retryBaseMs * 2 ** exponent;

        return Math.min (this.options.retryMaxMs, delay);
    }

    /**
        Arm `slot` (or disarm with `null`).

        The store applies the highlight optimistically, synchronously, before
        the request leaves. A failed request rolls the highlight back to the
        last confirmed state. Returns `false` without a request when the slot
        is outside the engine's slot list.
    */
    armSlot = async (slot: number | null): Promise<boolean> =>
    {
        const target = slot === null ? -1 : slot;
        const previous = this.snapshot.state;

        if (previous !== null && target >= previous.slots.length)
            return false;

        const sequence = this.armSequence + 1;
        this.armSequence = sequence;
        this.armInFlight += 1;

        if (previous !== null)
            this.patch ({ state: withArmedSlot (previous, target), error: null });

        try
        {
            const state = await this.api.armSlot (slot);

            if (sequence === this.armSequence)
            {
                this.patch ({
                    phase: "ready",
                    state,
                    error: null,
                    failedAttempts: 0,
                    updatedAt: this.options.now(),
                });
            }

            return true;
        }
        catch (error)
        {
            if (sequence === this.armSequence)
            {
                this.patch ({
                    error: messageOf (error),
                    ...(previous === null ? {} : { state: previous }),
                });
            }

            return false;
        }
        finally
        {
            this.armInFlight -= 1;
        }
    };

    private async load (): Promise<boolean>
    {
        if (this.snapshot.state === null)
            this.patch ({ phase: "connecting" });

        try
        {
            let state = await this.api.getState();

            // A poll that lands while an arm request is in flight may still
            // describe the old slot. Keep the optimistic highlight until the
            // arm response (or its rollback) arrives.
            if (this.armInFlight > 0 && this.snapshot.state !== null)
                state = withArmedSlot (state, this.snapshot.state.armedSlot);

            this.patch ({
                phase: "ready",
                state,
                error: null,
                failedAttempts: 0,
                updatedAt: this.options.now(),
            });

            return true;
        }
        catch (error)
        {
            this.patch ({
                phase: "offline",
                error: messageOf (error),
                failedAttempts: this.snapshot.failedAttempts + 1,
            });

            return false;
        }
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

    private patch (change: Partial<AppSnapshot>): void
    {
        this.snapshot = { ...this.snapshot, ...change };

        for (const listener of this.listeners)
            listener();
    }
}
