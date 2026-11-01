import { sendJson } from "../http";
import type { FixtureRoute } from "../types";

export const handleAudioMeasure: FixtureRoute = (
  context,
  request,
  response,
) => {
  if (request.method !== "POST" || request.url !== "/audio/measure")
    return false;

  context.measureRequests += 1;

  const expectedSamples = context.audio.bufferSize * 2;
  const roundTripSamples = expectedSamples;
  const expectedMs = (expectedSamples * 1000) / context.audio.sampleRate;

  sendJson(response, 200, {
    valid: true,
    roundTripSamples,
    roundTripMs: (roundTripSamples * 1000) / context.audio.sampleRate,
    expectedMs,
    confidence: 0.94,
    sampleRate: context.audio.sampleRate,
    bufferSize: context.audio.bufferSize,
    error: "",
  });

  return true;
};
