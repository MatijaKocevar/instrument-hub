import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleState: FixtureRoute = (context, request, response) => {
  if (request.method !== "GET" || request.url !== "/state") return false;

  context.stateRequests += 1;

  if (context.stateRequests <= context.failFirst) {
    sendJson(response, 503, { error: "engine is warming up" });
    return true;
  }

  sendJson(response, 200, context.state);
  return true;
};
