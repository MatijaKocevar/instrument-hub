import {
  assertLoopbackApiUrl,
  readOnlyMode,
  remoteApiAllowed,
} from "./security.js";

export interface HubConfig {
  apiUrl: string;
  token?: string;
  readOnly: boolean;
}

const DEFAULT_API_URL = "http://127.0.0.1:8080";

export function loadConfig(env: NodeJS.ProcessEnv = process.env): HubConfig {
  const apiUrl = normalizeApiUrl(env.IHUB_API_URL);
  assertLoopbackApiUrl(apiUrl, remoteApiAllowed(env));
  const token = env.IHUB_TOKEN?.trim();
  const readOnly = readOnlyMode(env);
  return token ? { apiUrl, token, readOnly } : { apiUrl, readOnly };
}

export function websocketUrl(apiUrl: string, path: string): string {
  const base = apiUrl.endsWith("/") ? apiUrl : `${apiUrl}/`;
  const url = new URL(path, base);
  url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
  return url.toString();
}

function normalizeApiUrl(raw: string | undefined): string {
  const value = raw?.trim() || DEFAULT_API_URL;
  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw new Error(`IHUB_API_URL is not a valid URL: ${value}`);
  }
  if (url.protocol !== "http:" && url.protocol !== "https:") {
    throw new Error(`IHUB_API_URL must use http or https: ${value}`);
  }
  return value.replace(/\/+$/, "");
}
