import type { ControlPatchSourceRequest } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";

export class PatchSourceActions {
  constructor(
    private readonly context: ActionContext,
    private readonly reloadChain: (slot: number) => Promise<boolean>,
  ) {}

  changeInstrumentSource = async (
    slot: number,
    source: ControlPatchSourceRequest,
  ): Promise<boolean> => {
    this.patch({ sourceBusy: true });

    try {
      const state = await this.context.api.changePatchSource(slot, source);

      this.patch({
        phase: "ready",
        state,
        sourceBusy: false,
        reloadBusy: false,
        reloadSlot: -1,
        captureBusy: false,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      await this.reloadChain(slot);
      return true;
    } catch (error) {
      this.patch({ sourceBusy: false });
      this.notify(
        failureToast(
          "Could not change the instrument source",
          error,
          `source:${String(slot)}`,
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
