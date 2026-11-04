import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type {
  ControlAudioMeasurement,
  ControlAudioPatch,
  ControlAudioState,
  ControlAudioWizard,
} from "./types";

export type MeasurementSource = "loopback" | "device";

export interface AudioEndpoints {
  getAudio(options?: { signal?: AbortSignal }): Promise<ControlAudioState>;

  updateAudio(
    patch: ControlAudioPatch,
    options?: { signal?: AbortSignal },
  ): Promise<ControlAudioState>;

  measureLatency(options?: {
    signal?: AbortSignal;
    source?: MeasurementSource;
  }): Promise<ControlAudioMeasurement>;

  recommendAudioSetup(options?: {
    signal?: AbortSignal;
  }): Promise<ControlAudioWizard>;

  applyAudioSetup(options?: {
    signal?: AbortSignal;
  }): Promise<ControlAudioState>;
}

function isAudioState(value: unknown): value is ControlAudioState {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlAudioState>;

  return (
    typeof candidate.open === "boolean" &&
    typeof candidate.deviceType === "string" &&
    typeof candidate.deviceName === "string" &&
    Array.isArray(candidate.deviceTypes) &&
    Array.isArray(candidate.devices) &&
    typeof candidate.sampleRate === "number" &&
    typeof candidate.bufferSize === "number" &&
    Array.isArray(candidate.sampleRates) &&
    Array.isArray(candidate.bufferSizes) &&
    Array.isArray(candidate.inputChannels) &&
    Array.isArray(candidate.outputChannels) &&
    typeof candidate.latency?.bufferMs === "number" &&
    typeof candidate.latency?.deviceMs === "number" &&
    typeof candidate.latency?.chainMs === "number" &&
    typeof candidate.latency?.totalMs === "number" &&
    Array.isArray(candidate.profiles) &&
    typeof candidate.xruns === "number"
  );
}

function isAudioWizard(value: unknown): value is ControlAudioWizard {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlAudioWizard>;

  return (
    typeof candidate.valid === "boolean" &&
    typeof candidate.error === "string" &&
    typeof candidate.deviceType === "string" &&
    typeof candidate.deviceName === "string" &&
    typeof candidate.soundcard === "string" &&
    typeof candidate.profile === "string" &&
    typeof candidate.sampleRate === "number" &&
    typeof candidate.bufferSize === "number" &&
    Array.isArray(candidate.inputChannels) &&
    Array.isArray(candidate.outputChannels) &&
    typeof candidate.latencyMs === "number" &&
    typeof candidate.measuredMs === "number" &&
    typeof candidate.expectedMs === "number" &&
    typeof candidate.xruns === "number" &&
    Array.isArray(candidate.candidates) &&
    typeof candidate.tradeoff === "string"
  );
}

function isAudioMeasurement(value: unknown): value is ControlAudioMeasurement {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlAudioMeasurement>;

  return (
    typeof candidate.valid === "boolean" &&
    typeof candidate.roundTripSamples === "number" &&
    typeof candidate.roundTripMs === "number" &&
    typeof candidate.expectedMs === "number" &&
    typeof candidate.confidence === "number" &&
    typeof candidate.sampleRate === "number" &&
    typeof candidate.bufferSize === "number" &&
    typeof candidate.error === "string"
  );
}

function requireAudioState(
  guard: RequestGuard,
  data: ControlAudioState | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlAudioState {
  if (data !== undefined && isAudioState(data)) return data;

  return guard.fail(error, status, what);
}

function requireAudioMeasurement(
  guard: RequestGuard,
  data: ControlAudioMeasurement | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlAudioMeasurement {
  if (data !== undefined && isAudioMeasurement(data)) return data;

  return guard.fail(error, status, what);
}

function requireAudioWizard(
  guard: RequestGuard,
  data: ControlAudioWizard | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlAudioWizard {
  if (data !== undefined && isAudioWizard(data)) return data;

  return guard.fail(error, status, what);
}

export function createAudioEndpoints(
  client: Client<paths>,
  guard: RequestGuard,
): AudioEndpoints {
  return {
    async getAudio(requestOptions) {
      const { data, error, response } = await client.GET("/audio", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireAudioState(
        guard,
        data,
        error,
        response.status,
        "GET /audio",
      );
    },

    async updateAudio(patch, requestOptions) {
      const { data, error, response } = await client.POST("/audio", {
        body: patch,
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireAudioState(
        guard,
        data,
        error,
        response.status,
        "POST /audio",
      );
    },

    async measureLatency(requestOptions) {
      const { data, error, response } = await client.POST("/audio/measure", {
        ...(requestOptions?.source !== undefined
          ? {
              params: {
                query: { source: requestOptions.source },
              },
            }
          : {}),
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireAudioMeasurement(
        guard,
        data,
        error,
        response.status,
        "POST /audio/measure",
      );
    },

    async recommendAudioSetup(requestOptions) {
      const { data, error, response } = await client.POST("/audio/wizard", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireAudioWizard(
        guard,
        data,
        error,
        response.status,
        "POST /audio/wizard",
      );
    },

    async applyAudioSetup(requestOptions) {
      const { data, error, response } = await client.POST(
        "/audio/wizard/apply",
        {
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireAudioState(
        guard,
        data,
        error,
        response.status,
        "POST /audio/wizard/apply",
      );
    },
  };
}
