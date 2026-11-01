import type { ControlPluginList } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Read and rescan the installed VST3 inventory.

    Busy/error flags live on the snapshot so the Plugins page and the source
    picker render the request state from the same polled view.
*/
export class PluginActions
{
    constructor (private readonly context: ActionContext) {}

    /** Read the installed VST3 inventory, optionally filtered by `search`. */
    loadPlugins = (search?: string): Promise<boolean> =>
        this.run (() => this.context.api.listPlugins (
            search === undefined || search.length === 0 ? {} : { search }));

    /** Ask the engine to rescan the VST3 directories. */
    rescanPlugins = (): Promise<boolean> =>
        this.run (() => this.context.api.rescanPlugins ());

    private async run (request: () => Promise<ControlPluginList>): Promise<boolean>
    {
        this.patch ({ pluginBusy: true, pluginError: null });

        try
        {
            const plugins = await request ();

            this.patch ({ plugins, pluginBusy: false, pluginError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ pluginBusy: false, pluginError: messageOf (error) });
            return false;
        }
    }

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
