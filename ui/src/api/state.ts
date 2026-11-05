import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type {
  ControlPatchEditor,
  ControlPatchSourceRequest,
  ControlPatchStateCapture,
  ControlPatchUpdate,
  ControlState,
  ControlTransport,
  ControlTransportPatch,
} from "./types";

export interface StateEndpoints {
  getState(options?: { signal?: AbortSignal }): Promise<ControlState>;

  armSlot(
    slot: number | null,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  selectTrack(
    index: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  renameTrack(
    index: number,
    name: string,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  toggleTrackFx(
    index: number,
    hot: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  assignTrackFx(
    index: number,
    hot: number,
    insertId: string,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  setTrackVolume(
    index: number,
    step: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  controlTransport(
    patch: ControlTransportPatch,
    options?: { signal?: AbortSignal },
  ): Promise<ControlTransport>;

  updatePatch(
    slot: number,
    patch: ControlPatchUpdate,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  changePatchSource(
    slot: number,
    source: ControlPatchSourceRequest,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  openPatchEditor(
    slot: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlPatchEditor>;

  closePatchEditor(
    slot: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlPatchEditor>;

  reloadPatch(
    slot: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlState>;

  capturePatchState(
    slot: number,
    options?: { signal?: AbortSignal },
  ): Promise<ControlPatchStateCapture>;
}

export function isControlState(value: unknown): value is ControlState {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlState>;

  return (
    candidate.schemaVersion === 1 &&
    typeof candidate.revision === "number" &&
    typeof candidate.application?.name === "string" &&
    typeof candidate.application?.version === "string" &&
    typeof candidate.armedSlot === "number" &&
    Array.isArray(candidate.slots) &&
    candidate.slots.every(
      (slot) =>
        slot !== null &&
        typeof slot === "object" &&
        typeof slot.index === "number" &&
        typeof slot.name === "string" &&
        typeof slot.armed === "boolean" &&
        typeof slot.latencySamples === "number" &&
        typeof slot.latencyMs === "number",
    ) &&
    (candidate.pages === undefined ||
      (Array.isArray(candidate.pages) &&
        candidate.pages.every(
          (page) =>
            page !== null &&
            typeof page === "object" &&
            typeof page.id === "string" &&
            typeof page.status === "string" &&
            Array.isArray(page.slots) &&
            typeof page.activeSlot === "number" &&
            typeof page.error === "string",
        ))) &&
    Array.isArray(candidate.families) &&
    candidate.families.every(
      (family) =>
        family !== null &&
        typeof family === "object" &&
        typeof family.id === "string" &&
        typeof family.name === "string" &&
        Array.isArray(family.patches) &&
        family.patches.every(
          (patch) =>
            patch !== null &&
            typeof patch === "object" &&
            typeof patch.slot === "number" &&
            typeof patch.name === "string" &&
            typeof patch.colorIndex === "number",
        ),
    ) &&
    typeof candidate.settings?.masterGain === "number" &&
    typeof candidate.transport?.bpm === "number"
  );
}

function isTransport(value: unknown): value is ControlTransport {
  if (value === null || typeof value !== "object") return false;

  const candidate = value as Partial<ControlTransport>;

  return (
    typeof candidate.playing === "boolean" &&
    typeof candidate.bpm === "number" &&
    typeof candidate.ppq === "number" &&
    typeof candidate.positionSamples === "number"
  );
}

function requireState(
  guard: RequestGuard,
  data: ControlState | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlState {
  if (data !== undefined) {
    if (!isControlState(data))
      return guard.reject(`${what} returned an unsupported payload`, status);

    return data;
  }

  return guard.fail(error, status, what);
}

function requireTransport(
  guard: RequestGuard,
  data: ControlTransport | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlTransport {
  if (data !== undefined && isTransport(data)) return data;

  return guard.fail(error, status, what);
}

function requireEditor(
  guard: RequestGuard,
  data: ControlPatchEditor | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlPatchEditor {
  if (
    data !== undefined &&
    typeof data.slot === "number" &&
    typeof data.open === "boolean"
  )
    return data;

  return guard.fail(error, status, what);
}

function requireStateCapture(
  guard: RequestGuard,
  data: ControlPatchStateCapture | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlPatchStateCapture {
  if (
    data !== undefined &&
    typeof data.slot === "number" &&
    typeof data.captured === "boolean" &&
    typeof data.sizeBytes === "number"
  )
    return data;

  return guard.fail(error, status, what);
}

export function createStateEndpoints(
  client: Client<paths>,
  guard: RequestGuard,
): StateEndpoints {
  return {
    async getState(requestOptions) {
      const { data, error, response } = await client.GET("/state", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireState(guard, data, error, response.status, "GET /state");
    },

    async armSlot(slot, requestOptions) {
      const { data, error, response } = await client.POST("/arm", {
        body: { slot },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireState(guard, data, error, response.status, "POST /arm");
    },

    async selectTrack(index, requestOptions) {
      const { data, error, response } = await client.POST(
        "/tracks/{index}/select",
        {
          params: { path: { index } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireState(
        guard,
        data,
        error,
        response.status,
        `POST /tracks/${String(index)}/select`,
      );
    },

    async renameTrack(index, name, requestOptions) {
      const { data, error, response } = await client.PATCH("/tracks/{index}", {
        params: { path: { index } },
        body: { name },
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireState(
        guard,
        data,
        error,
        response.status,
        `PATCH /tracks/${String(index)}`,
      );
    },

    async toggleTrackFx(index, hot, requestOptions) {
      const { data, error, response } = await client.POST(
        "/tracks/{index}/fx/{hot}/toggle",
        {
          params: { path: { index, hot } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireState(
        guard,
        data,
        error,
        response.status,
        `POST /tracks/${String(index)}/fx/${String(hot)}/toggle`,
      );
    },

    async assignTrackFx(index, hot, insertId, requestOptions) {
      const { data, error, response } = await client.POST(
        "/tracks/{index}/fx/{hot}/assign",
        {
          params: { path: { index, hot } },
          body: { insertId },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireState(
        guard,
        data,
        error,
        response.status,
        `POST /tracks/${String(index)}/fx/${String(hot)}/assign`,
      );
    },

    async setTrackVolume(index, step, requestOptions) {
      const { data, error, response } = await client.POST(
        "/tracks/{index}/volume",
        {
          params: { path: { index } },
          body: { step },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireState(
        guard,
        data,
        error,
        response.status,
        `POST /tracks/${String(index)}/volume`,
      );
    },

    async controlTransport(patch, requestOptions) {
      const { data, error, response } = await client.POST("/transport", {
        body: patch,
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireTransport(
        guard,
        data,
        error,
        response.status,
        "POST /transport",
      );
    },

    async updatePatch(slot, patch, requestOptions) {
      const { data, error, response } = await client.PATCH("/patches/{slot}", {
        params: { path: { slot } },
        body: patch,
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireState(
        guard,
        data,
        error,
        response.status,
        `PATCH /patches/${String(slot)}`,
      );
    },

    async changePatchSource(slot, source, requestOptions) {
      const { data, error, response } = await client.POST(
        "/patches/{slot}/source",
        {
          params: { path: { slot } },
          body: source,
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireState(
        guard,
        data,
        error,
        response.status,
        `POST /patches/${String(slot)}/source`,
      );
    },

    async openPatchEditor(slot, requestOptions) {
      const { data, error, response } = await client.POST(
        "/patches/{slot}/editor",
        {
          params: { path: { slot } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireEditor(
        guard,
        data,
        error,
        response.status,
        `POST /patches/${String(slot)}/editor`,
      );
    },

    async closePatchEditor(slot, requestOptions) {
      const { data, error, response } = await client.DELETE(
        "/patches/{slot}/editor",
        {
          params: { path: { slot } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireEditor(
        guard,
        data,
        error,
        response.status,
        `DELETE /patches/${String(slot)}/editor`,
      );
    },

    async reloadPatch(slot, requestOptions) {
      const { data, error, response } = await client.POST(
        "/patches/{slot}/reload",
        {
          params: { path: { slot } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireState(
        guard,
        data,
        error,
        response.status,
        `POST /patches/${String(slot)}/reload`,
      );
    },

    async capturePatchState(slot, requestOptions) {
      const { data, error, response } = await client.POST(
        "/patches/{slot}/state/capture",
        {
          params: { path: { slot } },
          ...(requestOptions?.signal !== undefined
            ? { signal: requestOptions.signal }
            : {}),
        },
      );

      return requireStateCapture(
        guard,
        data,
        error,
        response.status,
        `POST /patches/${String(slot)}/state/capture`,
      );
    },
  };
}
