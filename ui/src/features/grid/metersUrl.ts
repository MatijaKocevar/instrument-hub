export function metersUrlFor(baseUrl: string, token?: string | null): string {
  try {
    const url = new URL(baseUrl);
    url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
    url.pathname = "/meters";
    url.search = "";
    url.hash = "";

    if (token !== undefined && token !== null && token.length > 0)
      url.searchParams.set("token", token);

    return url.toString();
  } catch {
    return "";
  }
}
