import type { Client } from "openapi-fetch";
import type { components, paths } from "./schema.js";
import { unwrap } from "./result.js";

type PatchSourceRequest = components["schemas"]["PatchSourceRequest"];
type PatchEditor = components["schemas"]["PatchEditor"];
type PatchStateCapture = components["schemas"]["PatchStateCapture"];
type State = components["schemas"]["State"];
type HubClient = Client<paths>;

export interface PatchApi {
  setPatchSource(slot: number, source: PatchSourceRequest): Promise<State>;
  openPatchEditor(slot: number): Promise<PatchEditor>;
  closePatchEditor(slot: number): Promise<PatchEditor>;
  capturePatchState(slot: number): Promise<PatchStateCapture>;
  reloadPatch(slot: number): Promise<State>;
}

export function createPatchApi(client: HubClient): PatchApi {
  return {
    setPatchSource: async (slot, source) =>
      unwrap(
        await client.POST("/patches/{slot}/source", {
          params: { path: { slot } },
          body: source,
        }),
      ),
    openPatchEditor: async (slot) =>
      unwrap(
        await client.POST("/patches/{slot}/editor", {
          params: { path: { slot } },
        }),
      ),
    closePatchEditor: async (slot) =>
      unwrap(
        await client.DELETE("/patches/{slot}/editor", {
          params: { path: { slot } },
        }),
      ),
    capturePatchState: async (slot) =>
      unwrap(
        await client.POST("/patches/{slot}/state/capture", {
          params: { path: { slot } },
        }),
      ),
    reloadPatch: async (slot) =>
      unwrap(
        await client.POST("/patches/{slot}/reload", {
          params: { path: { slot } },
        }),
      ),
  };
}
