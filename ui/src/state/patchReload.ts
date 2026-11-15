import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";

export class PatchReloadActions {
  constructor(private readonly context: ActionContext) {}

  reloadInstrumentPlugin = async (slot: number): Promise<boolean> => {
    this.patch({
      reloadBusy: true,
      reloadSlot: slot,
    });

    try {
      const state = await this.context.api.reloadPatch(slot);

      this.patch({
        phase: "ready",
        state,
        reloadBusy: false,
        reloadSlot: -1,
        editorBusy: false,
        error: null,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      this.notify({
        severity: "success",
        title: "Plug-in reloaded",
        key: `reload:${String(slot)}`,
      });

      return true;
    } catch (error) {
      this.patch({
        reloadBusy: false,
        reloadSlot: -1,
      });
      this.notify(
        failureToast(
          "Could not reload the plug-in",
          error,
          `reload:${String(slot)}`,
        ),
      );
      return false;
    }
  };

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
