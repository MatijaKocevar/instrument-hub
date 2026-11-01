import { recalculateChain } from "../../fixtures";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

const fxIndexMatchPattern = /^\/patches\/(\d+)\/fx\/(\d+)$/;

export const handleFxInsert: FixtureRoute = (context, request, response) => {
  const match = fxIndexMatchPattern.exec(request.url ?? "");

  if (match === null) return false;

  const slot = Number.parseInt(match[1] ?? "", 10);
  const index = Number.parseInt(match[2] ?? "", 10);
  const chain = context.chainFor(slot);

  if (index < 0 || index >= chain.inserts.length) {
    sendJson(response, 404, { error: "fx index is out of range" });
    return true;
  }

  if (request.method === "DELETE") {
    chain.inserts.splice(index, 1);
    recalculateChain(chain);
    context.fxRequests.push({ method: "DELETE", slot, index, body: {} });
    sendJson(response, 200, chain);
    return true;
  }

  if (request.method === "PATCH") {
    readJsonObject(request, response, (body) => {
      const insert = chain.inserts[index];

      if (insert === undefined) {
        sendJson(response, 404, { error: "fx index is out of range" });
        return;
      }

      if (typeof body.bypassed === "boolean") insert.bypassed = body.bypassed;

      if (typeof body.wet === "number") {
        insert.wet = body.wet;
        insert.dry = 1 - body.wet;
      }

      if (
        typeof body.position === "number" &&
        Number.isInteger(body.position) &&
        body.position >= 0 &&
        body.position < chain.inserts.length &&
        body.position !== index
      ) {
        chain.inserts.splice(index, 1);
        chain.inserts.splice(body.position, 0, insert);
      }

      recalculateChain(chain);
      context.fxRequests.push({ method: "PATCH", slot, index, body });
      sendJson(response, 200, chain);
    });

    return true;
  }

  return false;
};
