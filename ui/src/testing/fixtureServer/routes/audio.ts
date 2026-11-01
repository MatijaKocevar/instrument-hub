import type { ControlAudioPatch, ControlAudioState } from "../../../api/client";
import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

function recalculateLatency(audio: ControlAudioState): void {
  const bufferMs = (audio.bufferSize * 1000) / Math.max(1, audio.sampleRate);

  audio.latency = {
    ...audio.latency,
    bufferMs,
    totalMs: bufferMs + audio.latency.deviceMs + audio.latency.chainMs,
  };
}

function handleAudioUpdate(
  context: Parameters<FixtureRoute>[0],
  request: Parameters<FixtureRoute>[1],
  response: Parameters<FixtureRoute>[2],
): boolean {
  readJsonObject(request, response, (body) => {
    const parsed = body as ControlAudioPatch;

    if (
      parsed.bufferSize !== undefined &&
      (!Number.isInteger(parsed.bufferSize) ||
        parsed.bufferSize < 16 ||
        parsed.bufferSize > 8192)
    ) {
      sendJson(response, 400, {
        error: "bufferSize must be between 16 and 8192",
      });
      return;
    }

    context.audioRequests.push(parsed);

    if (parsed.deviceType !== undefined)
      context.audio.deviceType = parsed.deviceType;

    if (parsed.deviceName !== undefined)
      context.audio.deviceName = parsed.deviceName;

    if (parsed.sampleRate !== undefined)
      context.audio.sampleRate = parsed.sampleRate;

    if (parsed.bufferSize !== undefined)
      context.audio.bufferSize = parsed.bufferSize;

    if (parsed.inputChannels !== undefined) {
      const active = new Set(parsed.inputChannels);

      context.audio.inputChannels = context.audio.inputChannels.map(
        (channel) => ({
          ...channel,
          active: active.has(channel.index),
        }),
      );
    }

    if (parsed.outputChannels !== undefined) {
      const active = new Set(parsed.outputChannels);

      context.audio.outputChannels = context.audio.outputChannels.map(
        (channel) => ({
          ...channel,
          active: active.has(channel.index),
        }),
      );
    }

    recalculateLatency(context.audio);

    sendJson(response, 200, context.audio);
  });

  return true;
}

export const handleAudio: FixtureRoute = (context, request, response) => {
  if (request.method === "GET" && request.url === "/audio") {
    sendJson(response, 200, context.audio);
    return true;
  }

  if (request.method === "POST" && request.url === "/audio")
    return handleAudioUpdate(context, request, response);

  return false;
};
