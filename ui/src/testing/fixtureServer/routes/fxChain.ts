// Insert-chain collection endpoints for the fixture server.

import type { ControlFxInsert } from "../../../api/client";
import { COMPRESSOR_LATENCY, recalculateChain } from "../../fixtures";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

const fxMatchPattern = /^\/patches\/(\d+)\/fx$/;

function insertFromRequest (body: Record<string, unknown>,
                            chainLength: number): ControlFxInsert
{
    const type = (typeof body.type === "string"
        ? body.type
        : "eq") as ControlFxInsert["type"];
    const wet = typeof body.wet === "number" ? body.wet : 1;
    const latency = type === "compressor" ? COMPRESSOR_LATENCY
        : type === "delay" ? 96
            : 0;

    return {
        index: chainLength,
        type,
        kind: type === "vst3" ? "vst3" : "builtin",
        id: typeof body.id === "string" && body.id.length > 0
            ? body.id
            : (typeof body.path === "string" ? body.path : type),
        name: typeof body.name === "string" && body.name.length > 0
            ? body.name
            : type,
        bypassed: body.bypassed === true,
        wet,
        dry: 1 - wet,
        latencySamples: latency,
        parameters: (body.parameters ?? {}) as Record<string, number>,
    };
}

export const handleFxChain: FixtureRoute = (context, request, response) =>
{
    const match = fxMatchPattern.exec (request.url ?? "");

    if (match === null)
        return false;

    const slot = Number.parseInt (match[1] ?? "", 10);
    const chain = context.chainFor (slot);

    if (request.method === "GET")
    {
        sendJson (response, 200, chain);
        return true;
    }

    if (request.method === "POST")
    {
        readJsonObject (request, response, body =>
        {
            const insert = insertFromRequest (body, chain.inserts.length);
            const position = typeof body.position === "number" ? body.position : -1;

            if (position < 0 || position >= chain.inserts.length)
                chain.inserts.push (insert);
            else
                chain.inserts.splice (position, 0, insert);

            recalculateChain (chain);
            context.fxRequests.push ({ method: "POST", slot, index: -1, body });
            sendJson (response, 200, chain);
        });

        return true;
    }

    if (request.method === "PUT")
    {
        readJsonObject (request, response, body =>
        {
            const requested = Array.isArray (body.chain) ? body.chain : [];

            chain.inserts = requested.map ((entry, index) =>
                insertFromRequest (entry as Record<string, unknown>, index));
            recalculateChain (chain);
            context.fxRequests.push ({ method: "PUT", slot, index: -1, body });
            sendJson (response, 200, chain);
        });

        return true;
    }

    return false;
};
