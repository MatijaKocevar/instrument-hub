import { formatStateSize } from "../features/instrument/format";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";

export class PatchCaptureActions {
  constructor(private readonly context: ActionContext) {}

  savePatchSound = async (slot: number): Promise<boolean> => {
    this.patch({ captureBusy: true });

    try {
      const capture = await this.context.api.capturePatchState(slot);

      this.patch({
        phase: "ready",
        captureBusy: false,
        error: null,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      this.notify({
        severity: "success",
        title: "Sound saved to patch",
        detail: formatStateSize(capture.sizeBytes),
        key: `capture:${String(slot)}`,
      });

      return true;
    } catch (error) {
      this.patch({ captureBusy: false });
      this.notify(
        failureToast(
          "Could not save the sound state",
          error,
          `capture:${String(slot)}`,
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
