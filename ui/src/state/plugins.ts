import type { ControlPluginList } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class PluginActions
{
    constructor (private readonly context: ActionContext) {}

    loadPlugins = (search?: string): Promise<boolean> =>
        this.run (() => this.context.api.listPlugins (
            search === undefined || search.length === 0 ? {} : { search }));

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
