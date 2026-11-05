import { findPatch } from "../context";
import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

const reloadMatchPattern = /^\/patches\/(\d+)\/reload$/;

export const handlePatchReload: FixtureRoute = (context, request, response) => {
  const match = reloadMatchPattern.exec(request.url ?? "");

  if (match === null || request.method !== "POST") return false;

  const slot = Number.parseInt(match[1] ?? "", 10);
  const target = findPatch(context, slot);

  context.reloadRequests.push({ slot });

  if (target === null) {
    sendJson(response, 404, { error: "patch slot is out of range" });
    return true;
  }

  if (target.instrument.kind !== "plugin") {
    sendJson(response, 400, { error: "patch slot does not host a plugin" });
    return true;
  }

  target.editorOpen = false;
  delete target.error;

  sendJson(response, 200, context.state);
  return true;
};
