import type { ControlFxInsert } from "../../../api/client";
import { recalculateChain } from "../../fixtures";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

const fxReorderMatchPattern = /^\/patches\/(\d+)\/fx\/reorder$/;

export const handleFxReorder: FixtureRoute = (context, request, response) => {
  if (request.method !== "POST") return false;

  const match = fxReorderMatchPattern.exec(request.url ?? "");

  if (match === null) return false;

  const slot = Number.parseInt(match[1] ?? "", 10);
  const chain = context.chainFor(slot);

  readJsonObject(request, response, (body) => {
    const order = Array.isArray(body.order) ? (body.order as number[]) : [];
    const reordered: ControlFxInsert[] = [];

    for (const index of order) {
      const insert = chain.inserts[index];

      if (insert !== undefined) reordered.push(insert);
    }

    if (reordered.length !== chain.inserts.length) {
      sendJson(response, 400, { error: "order must be a permutation" });
      return;
    }

    chain.inserts = reordered;
    recalculateChain(chain);
    context.fxRequests.push({ method: "POST", slot, index: -1, body });
    sendJson(response, 200, chain);
  });

  return true;
};
