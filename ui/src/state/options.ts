import type { TokenStorage } from "../features/pairing/model";

export interface AppStoreOptions
{
    pollIntervalMs?: number;
    retryBaseMs?: number;
    retryMaxMs?: number;
    now?: () => number;
    storage?: TokenStorage | null;
}

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

export function resolveAppStoreOptions (options: AppStoreOptions = {}): ResolvedAppStoreOptions
{
    return { ...DEFAULT_OPTIONS, ...options };
}
