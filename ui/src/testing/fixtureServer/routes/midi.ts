import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleMidi: FixtureRoute = (context, request, response) => {
  if (request.method === "GET" && request.url === "/midi") {
    sendJson(response, 200, context.midi);
    return true;
  }

  if (request.method === "POST" && request.url === "/midi/inputs") {
    readJsonObject(request, response, (body) => {
      if (typeof body.id !== "string") {
        sendJson(response, 400, { error: "id is required" });
        return;
      }

      const id = body.id;
      const enabled =
        typeof body.enabled === "boolean" ? body.enabled : undefined;
      const role = typeof body.role === "string" ? body.role : undefined;
      const noteBase =
        typeof body.noteBase === "number" ? body.noteBase : undefined;
      const learn = typeof body.learn === "boolean" ? body.learn : undefined;

      if (
        enabled === undefined &&
        role === undefined &&
        noteBase === undefined &&
        learn === undefined
      ) {
        sendJson(response, 400, {
          error: "enabled, role, noteBase or learn is required",
        });
        return;
      }

      const target = context.midi.inputs.find((input) => input.id === id);

      if (target === undefined) {
        sendJson(response, 404, { error: "unknown MIDI input" });
        return;
      }

      context.midiRequests.push({
        id,
        ...(enabled !== undefined ? { enabled } : {}),
        ...(role !== undefined ? { role: role as "keyboard" | "grid" } : {}),
        ...(noteBase !== undefined ? { noteBase } : {}),
        ...(learn !== undefined ? { learn } : {}),
      });

      if (enabled !== undefined) {
        target.enabled = enabled;
        target.open = enabled;

        if (target.role === "launchpad") target.handshake = enabled;
      }

      if (role !== undefined) {
        if (role === "grid") {
          target.role = "grid";
          target.noteBase = -1;
          target.learnedNotes = 0;
          target.learnActive = false;
          target.enabled = true;
          target.open = true;
        } else {
          target.role = "keyboard";
          target.noteBase = -1;
          target.learnedNotes = 0;
          target.learnActive = false;
        }
      }

      if (noteBase !== undefined) target.noteBase = noteBase;

      if (learn !== undefined) {
        target.learnActive = learn;
        target.learnedNotes = 0;

        if (learn) {
          target.role = "grid";
          target.noteBase = -1;
          target.enabled = true;
          target.open = true;
        }
      }

      sendJson(response, 200, context.midi);
    });

    return true;
  }

  if (request.method === "POST" && request.url === "/midi/pad-test") {
    context.padTestRequests += 1;
    context.midi.padTest.active = true;
    context.midi.padTest.lit = 64;

    sendJson(response, 200, context.midi);
    return true;
  }

  return false;
};
