// HTTP plumbing shared by the fixture server and its route modules.

import type { IncomingMessage, Server, ServerResponse } from "node:http";
import type { AddressInfo } from "node:net";

export function listen (server: Server): Promise<number>
{
    return new Promise ((resolve, reject) =>
    {
        server.once ("error", reject);
        server.listen (0, "127.0.0.1", () =>
        {
            const address = server.address ();

            if (address === null || typeof address === "string")
            {
                reject (new Error ("fixture server did not report a TCP address"));
                return;
            }

            resolve ((address as AddressInfo).port);
        });
    });
}

export function sendJson (response: ServerResponse, status: number, payload: unknown): void
{
    response.statusCode = status;
    response.end (JSON.stringify (payload));
}

/** Read a JSON object; malformed or non-object bodies become `{}`. */
export function readJsonObject (request: IncomingMessage,
                                response: ServerResponse,
                                done: (parsed: Record<string, unknown>) => void): void
{
    let body = "";

    request.on ("data", (chunk: Buffer) => { body += chunk.toString ("utf8"); });
    request.on ("end", () =>
    {
        try
        {
            const parsed = JSON.parse (body) as unknown;

            done (parsed !== null && typeof parsed === "object" && ! Array.isArray (parsed)
                ? parsed as Record<string, unknown>
                : {});
        }
        catch
        {
            sendJson (response, 400, { error: "body must be a JSON object" });
        }
    });
}
