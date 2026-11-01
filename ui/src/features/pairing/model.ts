import type { ControlPairingState } from "../../api/client";

export const PAIRING_TOKEN_KEY = "instrument-hub.pairing.token";

export interface TokenStorage {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
  removeItem(key: string): void;
}

function defaultStorage(): TokenStorage | null {
  if (typeof localStorage === "undefined") return null;

  return localStorage;
}

export function loadPairingToken(
  storage: TokenStorage | null = defaultStorage(),
): string | null {
  if (storage === null) return null;

  try {
    const value = storage.getItem(PAIRING_TOKEN_KEY);

    return value !== null && value.length > 0 ? value : null;
  } catch {
    return null;
  }
}

export function savePairingToken(
  token: string,
  storage: TokenStorage | null = defaultStorage(),
): void {
  if (storage === null || token.length === 0) return;

  try {
    storage.setItem(PAIRING_TOKEN_KEY, token);
  } catch {}
}

export function clearPairingToken(
  storage: TokenStorage | null = defaultStorage(),
): void {
  if (storage === null) return;

  try {
    storage.removeItem(PAIRING_TOKEN_KEY);
  } catch {}
}

export function normalizePairingCode(input: string): string {
  return input.replace(/\D/g, "").slice(0, 6);
}

export function isPairingCode(input: string): boolean {
  return /^[0-9]{6}$/.test(input);
}

export function codeFromSearch(search: string): string | null {
  const query = search.startsWith("?") ? search.slice(1) : search;

  if (query.length === 0) return null;

  for (const part of query.split("&")) {
    const separator = part.indexOf("=");

    if (separator < 0) continue;

    const key = decodeURIComponent(part.slice(0, separator));

    if (key !== "code") continue;

    const value = normalizePairingCode(
      decodeURIComponent(part.slice(separator + 1)),
    );

    return isPairingCode(value) ? value : null;
  }

  return null;
}

export function pairingPayload(pairing: ControlPairingState | null): string {
  return pairing?.payload ?? "";
}

export function formatCodeLifetime(milliseconds: number): string {
  if (!Number.isFinite(milliseconds) || milliseconds <= 0) return "expired";

  const totalSeconds = Math.ceil(milliseconds / 1000);
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = totalSeconds % 60;

  return `${String(minutes)}:${String(seconds).padStart(2, "0")}`;
}

export function spacedPairingCode(code: string): string {
  if (code.length !== 6) return code;

  return `${code.slice(0, 3)} ${code.slice(3)}`;
}

export function isPairingRemotePath(pathname: string): boolean {
  return pathname === "/remote" || pathname === "/remote/";
}
