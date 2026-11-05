import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class PatchReloadActions {
  constructor(private readonly context: ActionContext) {}

  reloadInstrumentPlugin = async (slot: number): Promise<boolean> => {
    this.patch({
      reloadBusy: true,
      reloadSlot: slot,
      editorError: null,
      editorErrorSlot: -1,
    });

    try {
      const state = await this.context.api.reloadPatch(slot);

      this.patch({
        phase: "ready",
        state,
        reloadBusy: false,
        reloadSlot: -1,
        editorOpenSlot: -1,
        editorBusy: false,
        editorError: null,
        editorErrorSlot: -1,
        error: null,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      return true;
    } catch (error) {
      this.patch({
        reloadBusy: false,
        reloadSlot: -1,
        editorError: messageOf(error),
        editorErrorSlot: slot,
      });
      return false;
    }
  };

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
