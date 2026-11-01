import type { TokenStorage } from "../features/pairing/model";

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
    /** Token persistence; defaults to localStorage when available. */
    storage?: TokenStorage | null;
}

/** Options after every default has been applied. */
export type ResolvedAppStoreOptions = Required<Omit<AppStoreOptions, "storage">> & {
    storage: TokenStorage | null;
};

const DEFAULT_OPTIONS: ResolvedAppStoreOptions = {
    pollIntervalMs: 2000,
    retryBaseMs: 250,
    retryMaxMs: 5000,
    now: () => Date.now(),
    storage: typeof localStorage === "undefined" ? null : localStorage,
};

/** Merge caller options with the defaults the store runs with. */
export function resolveAppStoreOptions (options: AppStoreOptions = {}): ResolvedAppStoreOptions
{
    return { ...DEFAULT_OPTIONS, ...options };
}
