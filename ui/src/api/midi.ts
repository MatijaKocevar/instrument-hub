import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type {
  ControlMidiInput,
  ControlMidiInputUpdate,
  ControlMidiState,
} from "./types";

export interface MidiEndpoints {
  getMidi(options?: { signal?: AbortSignal }): Promise<ControlMidiState>;

  updateMidiInput(
    patch: ControlMidiInputUpdate,
    options?: { signal?: AbortSignal },
  ): Promise<ControlMidiState>;

  setMidiInput(
    id: string,
    enabled: boolean,
    options?: { signal?: AbortSignal },
  ): Promise<ControlMidiState>;
}

function isMidiInput(value: unknown): value is ControlMidiInput {
  if (value === null || typeof value !== "object") return false;

  const input = value as Partial<ControlMidiInput>;

  return (
    typeof input.id === "string" &&
    typeof input.name === "string" &&
    (input.role === "launchpad" ||
      input.role === "keyboard" ||
      input.role === "grid") &&
    typeof input.port === "string" &&
    typeof input.enabled === "boolean" &&
    typeof input.open === "boolean" &&
    typeof input.handshake === "boolean" &&
    typeof input.leds === "boolean" &&
    typeof input.noteBase === "number" &&
    typeof input.learnedNotes === "number" &&
    typeof input.learnActive === "boolean" &&
    typeof input.events === "number" &&
    typeof input.lastEvent === "string" &&
    typeof input.error === "string"
  );
}

function isMidiState(value: unknown): value is ControlMidiState {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlMidiState>;

  return (
    Array.isArray(candidate.inputs) &&
    candidate.inputs.every(isMidiInput) &&
    typeof candidate.inputCount === "number" &&
    typeof candidate.error === "string"
  );
}

function requireMidiState(
  guard: RequestGuard,
  data: ControlMidiState | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlMidiState {
  if (data !== undefined && isMidiState(data)) return data;

  return guard.fail(error, status, what);
}

export function createMidiEndpoints(
  client: Client<paths>,
  guard: RequestGuard,
): MidiEndpoints {
  return {
    async getMidi(requestOptions) {
      const { data, error, response } = await client.GET("/midi", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireMidiState(guard, data, error, response.status, "GET /midi");
    },

    async updateMidiInput(patch, requestOptions) {
      const { data, error, response } = await client.POST("/midi/inputs", {
        body: patch,
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireMidiState(
        guard,
        data,
        error,
        response.status,
        "POST /midi/inputs",
      );
    },

    async setMidiInput(id, enabled, requestOptions) {
      const { data, error, response } = await client.POST("/midi/inputs", {
        body: { id, enabled },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireMidiState(
        guard,
        data,
        error,
        response.status,
        "POST /midi/inputs",
      );
    },

  };
}
