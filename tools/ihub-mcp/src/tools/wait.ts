import type { HubApi } from "../api/client.js";
import type { components } from "../api/schema.js";

type PluginPage = components["schemas"]["PluginPage"];

export interface PageReadyResult {
  page: PluginPage;
  waitedMs: number;
  revision: number;
}

export async function waitForPageReady(
  api: HubApi,
  pageId: string | undefined,
  timeoutMs: number,
): Promise<PageReadyResult> {
  const started = Date.now();
  for (;;) {
    const state = await api.getState();
    const pages = state.pages ?? [];
    const page = pageId
      ? pages.find((candidate) => candidate.id === pageId)
      : pages[0];

    if (page?.status === "ready") {
      return {
        page,
        waitedMs: Date.now() - started,
        revision: state.revision,
      };
    }

    const elapsed = Date.now() - started;
    if (elapsed >= timeoutMs) {
      const status = page?.status ?? "missing";
      const name = pageId ?? "(first page)";
      throw new Error(`${name} was not ready after ${timeoutMs} ms (status: ${status})`);
    }

    await sleep(Math.min(200, timeoutMs - elapsed));
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
