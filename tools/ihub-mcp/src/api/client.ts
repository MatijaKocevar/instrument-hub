import createClient from "openapi-fetch";
import type { HubConfig } from "../config.js";
import type { components, paths } from "./schema.js";

type State = components["schemas"]["State"];
type ProjectOverview = components["schemas"]["ProjectOverview"];
type AudioState = components["schemas"]["AudioState"];
type MidiState = components["schemas"]["MidiState"];
type PluginList = components["schemas"]["PluginList"];
type Transport = components["schemas"]["Transport"];
type TransportPatch = components["schemas"]["TransportPatch"];
type MidiInputUpdate = components["schemas"]["MidiInputUpdate"];

export interface HubApi {
  getState(): Promise<State>;
  getProject(): Promise<ProjectOverview>;
  getAudio(): Promise<AudioState>;
  getMidi(): Promise<MidiState>;
  getPlugins(search?: string): Promise<PluginList>;
  getTransport(): Promise<Transport>;
  selectTrack(index: number): Promise<State>;
  armSlot(slot: number | null): Promise<State>;
  setTrackVolume(index: number, step: number): Promise<State>;
  toggleTrackFx(index: number, hot: number): Promise<State>;
  setTransport(patch: TransportPatch): Promise<Transport>;
  setMidiInput(update: MidiInputUpdate): Promise<MidiState>;
}

export class HubApiError extends Error {
  readonly status: number;

  constructor(status: number, message: string) {
    super(message);
    this.name = "HubApiError";
    this.status = status;
  }
}

export function createHubApi(config: HubConfig): HubApi {
  const client = createClient<paths>({
    baseUrl: config.apiUrl,
    ...(config.token
      ? { headers: { "X-Instrument-Hub-Token": config.token } }
      : {}),
  });

  return {
    getState: async () => unwrap(await client.GET("/state")),
    getProject: async () => unwrap(await client.GET("/project")),
    getAudio: async () => unwrap(await client.GET("/audio")),
    getMidi: async () => unwrap(await client.GET("/midi")),
    getPlugins: async (search?: string) =>
      unwrap(
        search
          ? await client.GET("/plugins", { params: { query: { search } } })
          : await client.GET("/plugins"),
      ),
    getTransport: async () => unwrap(await client.GET("/transport")),
    selectTrack: async (index: number) =>
      unwrap(
        await client.POST("/tracks/{index}/select", {
          params: { path: { index } },
        }),
      ),
    armSlot: async (slot: number | null) =>
      unwrap(await client.POST("/arm", { body: { slot } })),
    setTrackVolume: async (index: number, step: number) =>
      unwrap(
        await client.POST("/tracks/{index}/volume", {
          params: { path: { index } },
          body: { step },
        }),
      ),
    toggleTrackFx: async (index: number, hot: number) =>
      unwrap(
        await client.POST("/tracks/{index}/fx/{hot}/toggle", {
          params: { path: { index, hot } },
        }),
      ),
    setTransport: async (patch: TransportPatch) =>
      unwrap(await client.POST("/transport", { body: patch })),
    setMidiInput: async (update: MidiInputUpdate) =>
      unwrap(await client.POST("/midi/inputs", { body: update })),
  };
}

function unwrap<T>(result: {
  data?: T;
  error?: unknown;
  response: Response;
}): T {
  if (result.data !== undefined) return result.data;
  throw new HubApiError(
    result.response.status,
    errorMessage(result.response, result.error),
  );
}

function errorMessage(response: Response, error: unknown): string {
  if (error && typeof error === "object" && "error" in error) {
    return String((error as { error: unknown }).error);
  }
  return response.statusText || `HTTP ${response.status}`;
}
