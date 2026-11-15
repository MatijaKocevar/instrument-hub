import type { ControlPatchEditor } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";
import { withEditorOpen } from "./patchModel";

export class PatchEditorActions {
  constructor(private readonly context: ActionContext) {}

  openInstrumentEditor = (slot: number): Promise<boolean> =>
    this.runEditor(slot, "Could not open the plug-in editor", (slotToOpen) =>
      this.context.api.openPatchEditor(slotToOpen),
    );

  closeInstrumentEditor = (slot: number): Promise<boolean> =>
    this.runEditor(slot, "Could not close the plug-in editor", (slotToClose) =>
      this.context.api.closePatchEditor(slotToClose),
    );

  private async runEditor(
    slot: number,
    failureTitle: string,
    request: (slot: number) => Promise<ControlPatchEditor>,
  ): Promise<boolean> {
    this.patch({ editorBusy: true });

    try {
      const result = await request(slot);
      const current = this.context.getSnapshot().state;

      this.patch({
        phase: "ready",
        state:
          current === null ? null : withEditorOpen(current, slot, result.open),
        editorBusy: false,
        error: null,
        failedAttempts: 0,
        updatedAt: this.context.now(),
      });

      return true;
    } catch (error) {
      this.patch({ editorBusy: false });
      this.notify(failureToast(failureTitle, error, `editor:${String(slot)}`));
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
