import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleArm: FixtureRoute = (context, request, response) => {
  if (request.method !== "POST" || request.url !== "/arm") return false;

  readJsonObject(request, response, (body) => {
    const raw = body.slot;
    const slot = typeof raw === "number" && Number.isInteger(raw) ? raw : null;

    if (raw !== undefined && raw !== null && slot === null) {
      sendJson(response, 400, { error: "slot must be an integer or null" });
      return;
    }

    context.armRequests.push({ slot });

    const target = slot === null || slot < 0 ? -1 : slot;

    if (target >= context.state.slots.length) {
      sendJson(response, 400, { error: "slot is out of range" });
      return;
    }

    const apply = (): void => {
      context.state.armedSlot = target;
      context.state.slots = context.state.slots.map((candidate) => ({
        ...candidate,
        armed: candidate.index === target,
      }));

      if (context.state.tracks !== undefined)
        context.state.tracks = context.state.tracks.map((track) => ({
          ...track,
          selected: track.slot === target,
        }));

      sendJson(response, 200, context.state);
    };

    if (context.armDelayMs > 0) setTimeout(apply, context.armDelayMs);
    else apply();
  });

  return true;
};
