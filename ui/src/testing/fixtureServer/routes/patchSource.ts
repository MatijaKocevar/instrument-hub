import type { ControlState } from "../../../api/client";
import { findPatch } from "../context";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

const sourceMatchPattern = /^\/patches\/(\d+)\/source$/;

type ControlPatch = ControlState["families"][number]["patches"][number];

export const handlePatchSource: FixtureRoute = (context, request, response) =>
{
    const match = sourceMatchPattern.exec (request.url ?? "");

    if (match === null || request.method !== "POST")
        return false;

    const slot = Number.parseInt (match[1] ?? "", 10);
    const target = findPatch (context, slot);

    readJsonObject (request, response, body =>
    {
        context.sourceRequests.push ({ slot, body });

        if (target === null)
        {
            sendJson (response, 404, { error: "patch slot is out of range" });
            return;
        }

        if (typeof body.kind !== "string"
            || ! ["empty", "init-tone", "sampler", "plugin", "audio-input"].includes (body.kind))
        {
            sendJson (response, 400, { error: "kind must be a source kind" });
            return;
        }

        const instrument: ControlPatch["instrument"] = {
            id: target.instrument.id,
            name: typeof body.name === "string" && body.name.length > 0
                ? body.name
                : target.instrument.name,
            kind: body.kind as ControlPatch["instrument"]["kind"],
        };

        for (const key of ["pluginFormat", "pluginId", "pluginName",
                           "pluginManufacturer", "pluginVersion",
                           "samplePath"] as const)
        {
            const value = body[key];

            if (typeof value === "string" && value.length > 0)
                Object.assign (instrument, { [key]: value });
        }

        target.instrument = instrument;
        target.editorOpen = false;

        sendJson (response, 200, context.state);
    });

    return true;
};
