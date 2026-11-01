import type { ControlTransportPatch } from "../../../api/client";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleTransport: FixtureRoute = (context, request, response) => {
  if (request.method !== "POST" || request.url !== "/transport") return false;

  readJsonObject(request, response, (body) => {
    const parsed = body as ControlTransportPatch;

    context.transportRequests.push(parsed);

    if (parsed.playing !== undefined)
      context.state.transport.playing = parsed.playing;

    if (parsed.bpm !== undefined) context.state.transport.bpm = parsed.bpm;

    if (parsed.ppq !== undefined) context.state.transport.ppq = parsed.ppq;

    if (parsed.positionSamples !== undefined)
      context.state.transport.positionSamples = parsed.positionSamples;

    sendJson(response, 200, context.state.transport);
  });

  return true;
};
