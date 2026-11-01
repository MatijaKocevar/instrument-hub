import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type { ControlPairResponse, ControlPairingState } from "./types";

export interface PairingEndpoints {
  getPairing(options?: { signal?: AbortSignal }): Promise<ControlPairingState>;

  setLanMode(
    enabled: boolean,
    options?: { signal?: AbortSignal },
  ): Promise<ControlPairingState>;

  pair(
    code: string,
    options?: { signal?: AbortSignal; clientName?: string },
  ): Promise<ControlPairResponse>;

  revokePairing(options?: {
    signal?: AbortSignal;
  }): Promise<ControlPairingState>;
}

function isPairingState(value: unknown): value is ControlPairingState {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlPairingState>;

  return (
    typeof candidate.lanEnabled === "boolean" &&
    typeof candidate.address === "string" &&
    typeof candidate.port === "number" &&
    typeof candidate.code === "string" &&
    typeof candidate.codeExpiresInMs === "number" &&
    typeof candidate.payload === "string" &&
    typeof candidate.pairedClients === "number"
  );
}

function isPairResponse(value: unknown): value is ControlPairResponse {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlPairResponse>;

  return (
    typeof candidate.token === "string" &&
    candidate.token.length > 0 &&
    typeof candidate.pairedClients === "number"
  );
}

function requirePairingState(
  guard: RequestGuard,
  data: ControlPairingState | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlPairingState {
  if (data !== undefined && isPairingState(data)) return data;

  return guard.fail(error, status, what);
}

function requirePairResponse(
  guard: RequestGuard,
  data: ControlPairResponse | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlPairResponse {
  if (data !== undefined && isPairResponse(data)) return data;

  return guard.fail(error, status, what);
}

export function createPairingEndpoints(
  client: Client<paths>,
  guard: RequestGuard,
): PairingEndpoints {
  return {
    async getPairing(requestOptions) {
      const { data, error, response } = await client.GET("/pairing", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requirePairingState(
        guard,
        data,
        error,
        response.status,
        "GET /pairing",
      );
    },

    async setLanMode(enabled, requestOptions) {
      const { data, error, response } = await client.POST("/pairing/lan", {
        body: { enabled },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requirePairingState(
        guard,
        data,
        error,
        response.status,
        "POST /pairing/lan",
      );
    },

    async pair(code, requestOptions) {
      const { data, error, response } = await client.POST("/pair", {
        body: {
          code,
          ...(requestOptions?.clientName !== undefined
            ? { clientName: requestOptions.clientName }
            : {}),
        },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requirePairResponse(
        guard,
        data,
        error,
        response.status,
        "POST /pair",
      );
    },

    async revokePairing(requestOptions) {
      const { data, error, response } = await client.POST("/pairing/revoke", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requirePairingState(
        guard,
        data,
        error,
        response.status,
        "POST /pairing/revoke",
      );
    },
  };
}
