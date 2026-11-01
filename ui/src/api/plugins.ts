import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type { ControlPluginList } from "./types";

/** Installed VST3 inventory endpoints. */
export interface PluginEndpoints
{
    /** Read the installed VST3 inventory, optionally filtered by `search`. */
    listPlugins (options?: { search?: string; signal?: AbortSignal }): Promise<ControlPluginList>;

    /** Start a background rescan and return the inventory as it was then. */
    rescanPlugins (options?: { signal?: AbortSignal }): Promise<ControlPluginList>;
}

/** Rejects inventory payloads the plugin browser cannot render. */
function isPluginList (value: unknown): value is ControlPluginList
{
    if (value === null || typeof value !== "object")
        return false;

    const candidate = value as Partial<ControlPluginList>;

    return Array.isArray (candidate.plugins)
        && typeof candidate.count === "number"
        && typeof candidate.total === "number"
        && typeof candidate.search === "string"
        && typeof candidate.scanning === "boolean"
        && typeof candidate.scanned === "boolean"
        && typeof candidate.lastScanMs === "number"
        && typeof candidate.error === "string"
        && candidate.plugins.every (plugin =>
               plugin !== null
               && typeof plugin === "object"
               && typeof plugin.id === "string"
               && typeof plugin.name === "string"
               && typeof plugin.vendor === "string"
               && typeof plugin.version === "string"
               && typeof plugin.category === "string"
               && typeof plugin.isInstrument === "boolean"
               && typeof plugin.isEffect === "boolean"
               && typeof plugin.format === "string"
               && typeof plugin.path === "string");
}

function requirePluginList (guard: RequestGuard,
                             data: ControlPluginList | undefined,
                             error: unknown,
                             status: number,
                             what: string): ControlPluginList
{
    if (data !== undefined)
    {
        if (! isPluginList (data))
            return guard.reject (`${what} returned an unsupported payload`, status);

        return data;
    }

    return guard.fail (error, status, what);
}

export function createPluginEndpoints (client: Client<paths>,
                                       guard: RequestGuard): PluginEndpoints
{
    return {
        async listPlugins (requestOptions)
        {
            const search = requestOptions?.search;

            const { data, error, response } = await client.GET ("/plugins", {
                ...(search !== undefined && search.length > 0 ? { params: { query: { search } } } : {}),
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requirePluginList (guard, data, error, response.status, "GET /plugins");
        },

        async rescanPlugins (requestOptions)
        {
            const { data, error, response } = await client.POST ("/plugins/rescan", {
                ...(requestOptions?.signal !== undefined ? { signal: requestOptions.signal } : {}),
            });

            return requirePluginList (guard, data, error, response.status, "POST /plugins/rescan");
        },
    };
}
