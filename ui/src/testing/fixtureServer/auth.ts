// Token auth for the fixture server, mirroring the engine's pairing rules.

import type { IncomingMessage, ServerResponse } from "node:http";

import { sendJson } from "./http";
import type { FixtureContext } from "./types";

function suppliedToken (request: IncomingMessage): string | null
{
    const value = request.headers["x-instrument-hub-token"];

    return typeof value === "string" ? value : null;
}

function authorized (context: FixtureContext, request: IncomingMessage): boolean
{
    return ! context.pairing.lanEnabled
        || suppliedToken (request) === context.pairingToken;
}

function isPairingManagement (url: string | undefined): boolean
{
    return url === "/pairing"
        || url === "/pairing/lan"
        || url === "/pairing/revoke";
}

/** Write a 401 when the request may not reach its route. */
export function rejectUnauthorized (context: FixtureContext,
                                    request: IncomingMessage,
                                    response: ServerResponse): boolean
{
    const management = isPairingManagement (request.url);

    if (context.pairing.lanEnabled && ! management && request.url !== "/pair"
        && ! authorized (context, request))
    {
        sendJson (response, 401, { error: "unauthorized" });
        return true;
    }

    if (management && context.pairing.lanEnabled && ! authorized (context, request))
    {
        sendJson (response, 401, { error: "unauthorized" });
        return true;
    }

    return false;
}
