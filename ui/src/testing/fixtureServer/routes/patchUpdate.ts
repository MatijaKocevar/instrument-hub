// Patch mix/colour update endpoints for the fixture server.

import { findPatch } from "../context";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handlePatchUpdate: FixtureRoute = (context, request, response) =>
{
    if (request.method !== "PATCH" || request.url?.startsWith ("/patches/") !== true)
        return false;

    const url = request.url;

    readJsonObject (request, response, parsed =>
    {
        const slot = Number.parseInt (url.slice ("/patches/".length), 10);

        context.patchRequests.push ({ slot, patch: parsed });

        if (! Number.isInteger (slot) || slot < 0)
        {
            sendJson (response, 404, { error: "patch slot is out of range" });
            return;
        }

        const target = findPatch (context, slot);

        if (target === null)
        {
            sendJson (response, 404, { error: "patch slot is out of range" });
            return;
        }

        if (parsed.colorIndex !== undefined)
        {
            const colour = parsed.colorIndex;

            if (typeof colour !== "number" || ! Number.isInteger (colour)
                || colour < 0 || colour > 15)
            {
                sendJson (response, 400, { error: "colorIndex must be 0..15" });
                return;
            }

            target.colorIndex = colour;
        }

        if (parsed.volumeDb !== undefined && typeof parsed.volumeDb === "number")
            target.volumeDb = parsed.volumeDb;

        if (parsed.pan !== undefined && typeof parsed.pan === "number")
            target.pan = parsed.pan;

        if (parsed.mute !== undefined && typeof parsed.mute === "boolean")
            target.mute = parsed.mute;

        if (parsed.solo !== undefined && typeof parsed.solo === "boolean")
            target.solo = parsed.solo;

        sendJson (response, 200, context.state);
    });

    return true;
};
