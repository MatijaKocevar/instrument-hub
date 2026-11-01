export type RemoteContrast = "high" | "standard";

export const DEFAULT_REMOTE_CONTRAST: RemoteContrast = "high";

export const REMOTE_CONTRAST_KEY = "instrument-hub.remote.contrast";

export interface ContrastStorage {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
}

export function isRemoteContrast(value: unknown): value is RemoteContrast {
  return value === "high" || value === "standard";
}

export function nextContrast(current: RemoteContrast): RemoteContrast {
  return current === "high" ? "standard" : "high";
}

export function contrastLabel(current: RemoteContrast): string {
  return current === "high" ? "High contrast" : "Standard";
}

export function loadRemoteContrast(
  storage: ContrastStorage | null,
): RemoteContrast {
  if (storage === null) return DEFAULT_REMOTE_CONTRAST;

  try {
    const stored = storage.getItem(REMOTE_CONTRAST_KEY);

    return isRemoteContrast(stored) ? stored : DEFAULT_REMOTE_CONTRAST;
  } catch {
    return DEFAULT_REMOTE_CONTRAST;
  }
}

export function saveRemoteContrast(
  storage: ContrastStorage | null,
  contrast: RemoteContrast,
): void {
  if (storage === null) return;

  try {
    storage.setItem(REMOTE_CONTRAST_KEY, contrast);
  } catch {}
}
