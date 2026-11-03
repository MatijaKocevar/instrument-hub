import type { IncomingMessage, ServerResponse } from "node:http";

import { rejectUnauthorized } from "./auth";
import { sendJson } from "./http";
import { handleArm } from "./routes/arm";
import { handleAudio } from "./routes/audio";
import { handleAudioMeasure } from "./routes/audioMeasure";
import { handleFxChain } from "./routes/fxChain";
import { handleFxInsert } from "./routes/fxInsert";
import { handleFxReorder } from "./routes/fxReorder";
import { handleMidi } from "./routes/midi";
import { handlePairing } from "./routes/pairing";
import { handlePatchCapture } from "./routes/patchCapture";
import { handlePatchEditor } from "./routes/patchEditor";
import { handlePatchSource } from "./routes/patchSource";
import { handlePatchUpdate } from "./routes/patchUpdate";
import { handlePlugins } from "./routes/plugins";
import { handleProject } from "./routes/project";
import { handleState } from "./routes/state";
import { handleTransport } from "./routes/transport";
import type { FixtureContext, FixtureRoute } from "./types";

const routes: readonly FixtureRoute[] = [
  handlePairing,
  handleFxChain,
  handleFxReorder,
  handleFxInsert,
  handlePatchEditor,
  handlePatchCapture,
  handlePatchSource,
  handlePlugins,
  handleMidi,
  handlePatchUpdate,
  handleState,
  handleProject,
  handleArm,
  handleTransport,
  handleAudio,
  handleAudioMeasure,
];

export function routeFixtureRequest(
  context: FixtureContext,
  request: IncomingMessage,
  response: ServerResponse,
): void {
  if (rejectUnauthorized(context, request, response)) return;

  for (const route of routes) {
    if (route(context, request, response)) return;
  }

  sendJson(response, 404, { error: "not found" });
}
