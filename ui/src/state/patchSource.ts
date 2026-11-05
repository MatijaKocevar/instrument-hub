import type { ControlPatchSourceRequest } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class PatchSourceActions {
  constructor(
    private readonly context: ActionContext,
    private readonly reloadChain: (slot: number) => Promise<boolean>,
  ) {}

  changeInstrumentSource = async (
    slot: number,
    source: ControlPatchSourceRequest,
  ): Promise<boolean> => {
    this.patch({ sourceBusy: true, error: null });

    try {
      const state = await this.context.api.changePatchSource(slot, source);

      this.patch({
        phase: "ready",
        state,
        sourceBusy: false,
        editorOpenSlot: -1,
        editorError: null,
        editorErrorSlot: -1,
        reloadBusy: false,
        reloadSlot: -1,
        captureBusy: false,
        captureSlot: -1,
        capture: null,
        captureError: null,
        error: null,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      await this.reloadChain(slot);
      return true;
    } catch (error) {
      this.patch({ sourceBusy: false, error: messageOf(error) });
      return false;
    }
  };

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
