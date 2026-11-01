// Explicit patch-sound capture endpoints for the fixture server.

import { findPatch } from "../context";
import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

const stateCaptureMatchPattern = /^\/patches\/(\d+)\/state\/capture$/;

export const handlePatchCapture: FixtureRoute = (context, request, response) =>
{
    const match = stateCaptureMatchPattern.exec (request.url ?? "");

    if (match === null || request.method !== "POST")
        return false;

    const slot = Number.parseInt (match[1] ?? "", 10);
    const target = findPatch (context, slot);

    context.stateCaptureRequests.push ({ slot });

    if (target === null)
    {
        sendJson (response, 404, { error: "patch slot is out of range" });
        return true;
    }

    if (target.instrument.kind !== "plugin")
    {
        sendJson (response, 400, { error: "the patch has no resident plugin sound to save" });
        return true;
    }

    sendJson (response, 200, { slot, captured: true, sizeBytes: 4096 + slot });
    return true;
};
