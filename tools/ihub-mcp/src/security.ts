const TRUTHY = new Set(["1", "true", "yes", "on"]);

export function envFlag(value: string | undefined): boolean {
  return value !== undefined && TRUTHY.has(value.trim().toLowerCase());
}

export function readOnlyMode(env: NodeJS.ProcessEnv): boolean {
  return envFlag(env.IHUB_MCP_READ_ONLY);
}

export function remoteApiAllowed(env: NodeJS.ProcessEnv): boolean {
  return envFlag(env.IHUB_MCP_ALLOW_REMOTE);
}

export function assertLoopbackApiUrl(
  apiUrl: string,
  allowRemote: boolean,
): void {
  if (allowRemote) return;
  const { hostname } = new URL(apiUrl);
  if (isLoopbackHost(hostname)) return;
  throw new Error(
    `IHUB_API_URL must use a loopback host (127.0.0.1, ::1 or localhost): ${apiUrl}; ` +
      "set IHUB_MCP_ALLOW_REMOTE=1 to allow a remote engine.",
  );
}

export function requireConfirmation(
  confirm: boolean | undefined,
  action: string,
): void {
  if (confirm !== true) {
    throw new Error(`${action} requires confirm: true.`);
  }
}

function isLoopbackHost(hostname: string): boolean {
  const host = hostname.toLowerCase();
  if (host === "localhost" || host === "localhost.") return true;
  if (host === "::1" || host === "[::1]") return true;
  return /^127(?:\.\d{1,3}){3}$/.test(host);
}
