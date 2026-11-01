import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class PatchCaptureActions {
  constructor(private readonly context: ActionContext) {}

  savePatchSound = async (slot: number): Promise<boolean> => {
    this.patch({
      captureBusy: true,
      captureSlot: -1,
      capture: null,
      captureError: null,
    });

    try {
      const capture = await this.context.api.capturePatchState(slot);

      this.patch({
        phase: "ready",
        captureBusy: false,
        captureSlot: slot,
        capture,
        captureError: null,
        error: null,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      return true;
    } catch (error) {
      this.patch({
        captureBusy: false,
        captureSlot: slot,
        capture: null,
        captureError: messageOf(error),
      });

      return false;
    }
  };

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
