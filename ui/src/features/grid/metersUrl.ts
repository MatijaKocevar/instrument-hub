/** Turn the control API origin into the `/meters` WebSocket URL. */
export function metersUrlFor (baseUrl: string): string
{
    try
    {
        const url = new URL (baseUrl);
        url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
        url.pathname = "/meters";
        url.search = "";
        url.hash = "";

        return url.toString ();
    }
    catch
    {
        return "";
    }
}
