export function stateEventsUrlFor(baseUrl: string): string {
  try {
    const url = new URL(baseUrl);
    url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
    url.pathname = "/events";
    url.search = "";
    url.hash = "";

    return url.toString();
  } catch {
    return "";
  }
}
