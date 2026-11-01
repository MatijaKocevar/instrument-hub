import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleMidi: FixtureRoute = (context, request, response) =>
{
    if (request.method === "GET" && request.url === "/midi")
    {
        sendJson (response, 200, context.midi);
        return true;
    }

    if (request.method === "POST" && request.url === "/midi/inputs")
    {
        readJsonObject (request, response, body =>
        {
            if (typeof body.id !== "string" || typeof body.enabled !== "boolean")
            {
                sendJson (response, 400, { error: "id and enabled are required" });
                return;
            }

            const id = body.id;
            const enabled = body.enabled;
            const target = context.midi.inputs.find (input => input.id === id);

            if (target === undefined)
            {
                sendJson (response, 404, { error: "unknown MIDI input" });
                return;
            }

            context.midiRequests.push ({ id, enabled });
            target.enabled = enabled;
            target.open = enabled;

            if (target.role === "launchpad")
            {
                context.midi.launchpad.connected = enabled;
                context.midi.launchpad.handshake = enabled;
            }

            if (target.role === "keystation")
            {
                context.midi.keystation.connected = enabled;
                context.midi.keystation.inputName = enabled ? target.name : "";
            }

            sendJson (response, 200, context.midi);
        });

        return true;
    }

    if (request.method === "POST" && request.url === "/midi/pad-test")
    {
        context.padTestRequests += 1;
        context.midi.padTest.active = true;
        context.midi.padTest.lit = 64;

        sendJson (response, 200, context.midi);
        return true;
    }

    return false;
};
