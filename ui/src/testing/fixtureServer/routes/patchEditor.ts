import { findPatch } from "../context";
import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

const editorMatchPattern = /^\/patches\/(\d+)\/editor$/;

export const handlePatchEditor: FixtureRoute = (context, request, response) =>
{
    const match = editorMatchPattern.exec (request.url ?? "");

    if (match === null || (request.method !== "POST" && request.method !== "DELETE"))
        return false;

    const slot = Number.parseInt (match[1] ?? "", 10);
    const target = findPatch (context, slot);
    const open = request.method === "POST";

    context.editorRequests.push ({ slot, open });

    if (target === null)
    {
        sendJson (response, 404, { error: "patch slot is out of range" });
        return true;
    }

    if (open && target.instrument.kind !== "plugin")
    {
        sendJson (response, 400, { error: "the patch has no resident plugin to edit" });
        return true;
    }

    const wasOpen = target.editorOpen === true;

    target.editorOpen = open;
    sendJson (response, 200, {
        slot,
        open,
        ...(open ? { reused: wasOpen, wasOpen } : { wasOpen }),
    });

    return true;
};
