import type { ControlPluginList } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast, messageOf } from "./errors";

export class PluginActions {
  constructor(private readonly context: ActionContext) {}

  loadPlugins = (search?: string): Promise<boolean> =>
    this.run("Could not load plug-ins", () =>
      this.context.api.listPlugins(
        search === undefined || search.length === 0 ? {} : { search },
      ),
    );

  rescanPlugins = (): Promise<boolean> =>
    this.run("Could not rescan plug-ins", () =>
      this.context.api.rescanPlugins(),
    );

  private async run(
    title: string,
    request: () => Promise<ControlPluginList>,
  ): Promise<boolean> {
    this.patch({ pluginBusy: true, pluginError: null });

    try {
      const plugins = await request();

      this.patch({ plugins, pluginBusy: false, pluginError: null });

      return true;
    } catch (error) {
      this.patch({ pluginBusy: false, pluginError: messageOf(error) });
      this.notify(failureToast(title, error, "plugin-scan"));
      return false;
    }
  }

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
