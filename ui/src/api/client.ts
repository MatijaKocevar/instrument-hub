import createClient from "openapi-fetch";

import { createAudioEndpoints, type AudioEndpoints } from "./audio";
import { createFxEndpoints, type FxEndpoints } from "./fx";
import { createMidiEndpoints, type MidiEndpoints } from "./midi";
import { createPairingEndpoints, type PairingEndpoints } from "./pairing";
import { createPluginEndpoints, type PluginEndpoints } from "./plugins";
import { createProjectEndpoints, type ProjectEndpoints } from "./project";
import type { paths } from "./schema";
import { createStateEndpoints, type StateEndpoints } from "./state";

export type {
  ControlAudioChannel,
  ControlAudioDevice,
  ControlAudioMeasurement,
  ControlAudioPatch,
  ControlAudioState,
  ControlFxChain,
  ControlFxInsert,
  ControlFxInsertRequest,
  ControlFxInsertUpdate,
  ControlMidiInput,
  ControlMidiInputUpdate,
  ControlMidiPadTest,
  ControlMidiPort,
  ControlMidiState,
  ControlPairResponse,
  ControlPairingState,
  ControlPatch,
  ControlPatchEditor,
  ControlPatchInstrument,
  ControlPatchSourceRequest,
  ControlPatchStateCapture,
  ControlPatchUpdate,
  ControlPlugin,
  ControlPluginList,
  ControlProjectCommand,
  ControlProjectOverview,
  ControlSettings,
  ControlSlot,
  ControlState,
  ControlTransport,
  ControlTransportPatch,
} from "./types";

export class ApiError extends Error {
  readonly status: number;

  constructor(message: string, status = 0) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

export interface ControlApiOptions {
  baseUrl: string;
  token?: string | null;
  fetch?: typeof globalThis.fetch;
}

export interface RequestGuard {
  fail(error: unknown, status: number, what: string): never;
  reject(message: string, status: number): never;
}

export interface ControlApi
  extends
    StateEndpoints,
    FxEndpoints,
    PluginEndpoints,
    MidiEndpoints,
    AudioEndpoints,
    PairingEndpoints,
    ProjectEndpoints {
  readonly baseUrl: string;

  readonly token: string | null;

  setToken(token: string | null): void;
}

function describeError(value: unknown): string {
  if (value !== null && typeof value === "object" && "error" in value) {
    const message = (value as { error?: unknown }).error;

    if (typeof message === "string" && message.length > 0) return message;
  }

  return "";
}

function fail(error: unknown, status: number, what: string): never {
  const message = describeError(error);

  throw new ApiError(
    message.length > 0 ? message : `${what} failed with HTTP ${status}`,
    status,
  );
}

function reject(message: string, status: number): never {
  throw new ApiError(message, status);
}

export function createControlApi(options: ControlApiOptions): ControlApi {
  const baseUrl = options.baseUrl.replace(/\/+$/, "");
  let tokenValue = options.token ?? null;

  const client = createClient<paths>({
    baseUrl,
    ...(options.fetch !== undefined ? { fetch: options.fetch } : {}),
  });

  client.use({
    onRequest({ request }) {
      if (tokenValue !== null && tokenValue.length > 0)
        request.headers.set("X-Instrument-Hub-Token", tokenValue);

      return request;
    },
  });

  const guard: RequestGuard = { fail, reject };

  return {
    baseUrl,

    get token() {
      return tokenValue;
    },

    setToken(token) {
      tokenValue = token !== null && token.length > 0 ? token : null;
    },

    ...createStateEndpoints(client, guard),
    ...createFxEndpoints(client, guard),
    ...createPluginEndpoints(client, guard),
    ...createMidiEndpoints(client, guard),
    ...createAudioEndpoints(client, guard),
    ...createPairingEndpoints(client, guard),
    ...createProjectEndpoints(client, guard),
  };
}
