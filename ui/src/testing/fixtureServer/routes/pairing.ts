import { readJsonObject, sendJson } from "../http";
import { PAIRING_OFF, PAIRING_ON } from "../../fixtures";
import type { FixtureRoute } from "../types";

export const handlePairing: FixtureRoute = (context, request, response) =>
{
    if (request.method === "GET" && request.url === "/pairing")
    {
        sendJson (response, 200, context.pairing);
        return true;
    }

    if (request.method === "POST" && request.url === "/pairing/lan")
    {
        readJsonObject (request, response, body =>
        {
            const enabled = body.enabled === true;

            context.lanRequests.push (enabled);

            if (enabled)
            {
                context.pairing = { ...PAIRING_ON };
                context.pairingToken = null;
            }
            else
            {
                context.pairing = { ...PAIRING_OFF };
                context.pairingToken = null;
            }

            sendJson (response, 200, context.pairing);
        });

        return true;
    }

    if (request.method === "POST" && request.url === "/pairing/revoke")
    {
        context.pairingToken = null;
        context.pairing = { ...context.pairing, pairedClients: 0 };
        sendJson (response, 200, context.pairing);
        return true;
    }

    if (request.method === "POST" && request.url === "/pair")
    {
        readJsonObject (request, response, body =>
        {
            const code = typeof body.code === "string" ? body.code : "";

            context.pairRequests.push (code);

            if (! context.pairing.lanEnabled)
            {
                sendJson (response, 403, { error: "LAN mode is off" });
                return;
            }

            if (code !== context.pairing.code)
            {
                sendJson (response, 401, { error: "invalid pairing code" });
                return;
            }

            context.pairingToken = "fixture-pairing-token";

            const rotatedCode = context.pairing.code === "246810" ? "135790" : "246810";

            context.pairing = {
                ...context.pairing,
                code: rotatedCode,
                codeExpiresInMs: 300_000,
                payload: `http://192.168.1.20:8080/remote?code=${rotatedCode}`,
                pairedClients: context.pairing.pairedClients + 1,
            };

            sendJson (response, 200, {
                token: context.pairingToken,
                pairedClients: context.pairing.pairedClients,
            });
        });

        return true;
    }

    return false;
};
