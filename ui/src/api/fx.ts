import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type {
  ControlFxChain,
  ControlFxInsertRequest,
  ControlFxInsertUpdate,
} from "./types";

export interface FxEndpoints {
  getFxChain(
    slot: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxChain>;

  addFxInsert(
    slot: number,
    insert: ControlFxInsertRequest,
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxChain>;

  replaceFxChain(
    slot: number,
    chain: readonly ControlFxInsertRequest[],
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxChain>;

  updateFxInsert(
    slot: number,
    index: number,
    update: ControlFxInsertUpdate,
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxChain>;

  removeFxInsert(
    slot: number,
    index: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxChain>;

  reorderFxChain(
    slot: number,
    order: readonly number[],
    options?: { signal?: AbortSignal },
  ): Promise<ControlFxChain>;
}

function isFxChain(value: unknown): value is ControlFxChain {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlFxChain>;

  return (
    typeof candidate.slot === "number" &&
    typeof candidate.instrumentLatencySamples === "number" &&
    typeof candidate.fxLatencySamples === "number" &&
    typeof candidate.latencySamples === "number" &&
    typeof candidate.latencyMs === "number" &&
    Array.isArray(candidate.inserts) &&
    candidate.inserts.every(
      (insert) =>
        insert !== null &&
        typeof insert === "object" &&
        typeof insert.index === "number" &&
        typeof insert.type === "string" &&
        typeof insert.kind === "string" &&
        typeof insert.id === "string" &&
        typeof insert.name === "string" &&
        typeof insert.bypassed === "boolean" &&
        typeof insert.wet === "number" &&
        typeof insert.latencySamples === "number" &&
        insert.parameters !== null &&
        typeof insert.parameters === "object",
    )
  );
}

function requireChain(
  guard: RequestGuard,
  data: ControlFxChain | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlFxChain {
  if (data !== undefined) {
    if (!isFxChain(data))
      return guard.reject(`${what} returned an unsupported payload`, status);

    return data;
  }

  return guard.fail(error, status, what);
}

export function createFxEndpoints(
  client: Client<paths>,
  guard: RequestGuard,
): FxEndpoints {
  return {
    async getFxChain(slot, requestOptions) {
      const { data, error, response } = await client.GET("/patches/{slot}/fx", {
        params: { path: { slot } },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireChain(
        guard,
        data,
        error,
        response.status,
        `GET /patches/${String(slot)}/fx`,
      );
    },

    async addFxInsert(slot, insert, requestOptions) {
      const { data, error, response } = await client.POST(
        "/patches/{slot}/fx",
        {
          params: { path: { slot } },
          body: insert,
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireChain(
        guard,
        data,
        error,
        response.status,
        `POST /patches/${String(slot)}/fx`,
      );
    },

    async replaceFxChain(slot, chain, requestOptions) {
      const { data, error, response } = await client.PUT("/patches/{slot}/fx", {
        params: { path: { slot } },
        body: { chain: [...chain] },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireChain(
        guard,
        data,
        error,
        response.status,
        `PUT /patches/${String(slot)}/fx`,
      );
    },

    async updateFxInsert(slot, index, update, requestOptions) {
      const { data, error, response } = await client.PATCH(
        "/patches/{slot}/fx/{index}",
        {
          params: { path: { slot, index } },
          body: update,
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireChain(
        guard,
        data,
        error,
        response.status,
        `PATCH /patches/${String(slot)}/fx/${String(index)}`,
      );
    },

    async removeFxInsert(slot, index, requestOptions) {
      const { data, error, response } = await client.DELETE(
        "/patches/{slot}/fx/{index}",
        {
          params: { path: { slot, index } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireChain(
        guard,
        data,
        error,
        response.status,
        `DELETE /patches/${String(slot)}/fx/${String(index)}`,
      );
    },

    async reorderFxChain(slot, order, requestOptions) {
      const { data, error, response } = await client.POST(
        "/patches/{slot}/fx/reorder",
        {
          params: { path: { slot } },
          body: { order: [...order] },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireChain(
        guard,
        data,
        error,
        response.status,
        `POST /patches/${String(slot)}/fx/reorder`,
      );
    },
  };
}
