import { initialSnapshot, type AppSnapshot } from "./snapshot";

/**
    Holds the current snapshot and notifies subscribers when it is patched.

    One responsibility: turning a partial change into the next immutable
    snapshot. There is no I/O and no domain knowledge here.
*/
export class StoreCore
{
    private readonly listeners = new Set<() => void>();
    private snapshot: AppSnapshot = initialSnapshot();

    getSnapshot = (): AppSnapshot => this.snapshot;

    subscribe = (listener: () => void): (() => void) =>
    {
        this.listeners.add (listener);

        return () =>
        {
            this.listeners.delete (listener);
        };
    };

    patch = (change: Partial<AppSnapshot>): void =>
    {
        this.snapshot = { ...this.snapshot, ...change };

        for (const listener of this.listeners)
            listener();
    };
}
