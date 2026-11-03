import type { TokenStorage } from "../features/pairing/model";
import {
  defaultStateSocketFactory,
  type StateSocketFactory,
} from "./stateSocket";

export interface AppStoreOptions {
  pollIntervalMs?: number;
  retryBaseMs?: number;
  retryMaxMs?: number;
  now?: () => number;
  storage?: TokenStorage | null;
  stateSocketFactory?: StateSocketFactory;
}

export type ResolvedAppStoreOptions = Required<
  Omit<AppStoreOptions, "storage">
> & {
  storage: TokenStorage | null;
};

const DEFAULT_OPTIONS: ResolvedAppStoreOptions = {
  pollIntervalMs: 2000,
  retryBaseMs: 250,
  retryMaxMs: 5000,
  now: () => Date.now(),
  storage: typeof localStorage === "undefined" ? null : localStorage,
  stateSocketFactory: defaultStateSocketFactory,
};

export function resolveAppStoreOptions(
  options: AppStoreOptions = {},
): ResolvedAppStoreOptions {
  return { ...DEFAULT_OPTIONS, ...options };
}
