const CLIENT_KEY = "instrument-hub.midi.client";

function defaultStorage(): Storage | null {
  if (typeof localStorage === "undefined") return null;

  return localStorage;
}

function randomId(): string {
  if (typeof crypto !== "undefined" && "randomUUID" in crypto)
    return crypto.randomUUID().replace(/-/g, "").slice(0, 24);

  return `${Math.random().toString(36).slice(2)}${Date.now().toString(36)}`;
}

export function loadMidiClientId(
  storage: Storage | null = defaultStorage(),
): string {
  if (storage !== null) {
    try {
      const stored = storage.getItem(CLIENT_KEY);

      if (stored !== null && /^[A-Za-z0-9_-]{1,64}$/.test(stored))
        return stored;
    } catch {}
  }

  const id = randomId();

  if (storage !== null) {
    try {
      storage.setItem(CLIENT_KEY, id);
    } catch {}
  }

  return id;
}
