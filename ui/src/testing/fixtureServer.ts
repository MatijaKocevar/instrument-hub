import { createServer } from "node:http";

import { createFixtureContext } from "./fixtureServer/context";
import { listen } from "./fixtureServer/http";
import { routeFixtureRequest } from "./fixtureServer/router";
import type { FixtureOptions, FixtureServer } from "./fixtureServer/types";

export type { FixtureOptions, FixtureServer } from "./fixtureServer/types";

export async function startFixtureServer(
  options: FixtureOptions = {},
): Promise<FixtureServer> {
  const context = createFixtureContext(options);
  const server = createServer((request, response) => {
    response.setHeader("content-type", "application/json");
    routeFixtureRequest(context, request, response);
  });

  const port = await listen(server);

  return {
    baseUrl: `http://127.0.0.1:${String(port)}`,
    get stateRequests() {
      return context.stateRequests;
    },
    get armRequests() {
      return context.armRequests;
    },
    get audioRequests() {
      return context.audioRequests;
    },
    get transportRequests() {
      return context.transportRequests;
    },
    get measureRequests() {
      return context.measureRequests;
    },
    get audio() {
      return context.audio;
    },
    get pairing() {
      return context.pairing;
    },
    get pairRequests() {
      return context.pairRequests;
    },
    get lanRequests() {
      return context.lanRequests;
    },
    get patchRequests() {
      return context.patchRequests;
    },
    get fxRequests() {
      return context.fxRequests;
    },
    get sourceRequests() {
      return context.sourceRequests;
    },
    get editorRequests() {
      return context.editorRequests;
    },
    get stateCaptureRequests() {
      return context.stateCaptureRequests;
    },
    get pluginRequests() {
      return context.pluginRequests;
    },
    get midiRequests() {
      return context.midiRequests;
    },
    get padTestRequests() {
      return context.padTestRequests;
    },
    get midi() {
      return context.midi;
    },
    get project() {
      return context.project;
    },
    get projectRequests() {
      return context.projectRequests;
    },
    get chain() {
      return context.chainFor(3);
    },
    close: () =>
      new Promise((resolve, reject) => {
        server.close((error) => {
          error === undefined ? resolve() : reject(error);
        });
      }),
  };
}
