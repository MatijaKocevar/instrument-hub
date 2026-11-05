import type { ControlPatchEditor } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";
import { withEditorOpen } from "./patchModel";

export class PatchEditorActions {
  constructor(private readonly context: ActionContext) {}

  openInstrumentEditor = (slot: number): Promise<boolean> =>
    this.runEditor(slot, (slotToOpen) =>
      this.context.api.openPatchEditor(slotToOpen),
    );

  closeInstrumentEditor = (slot: number): Promise<boolean> =>
    this.runEditor(slot, (slotToClose) =>
      this.context.api.closePatchEditor(slotToClose),
    );

  clearInstrumentError = (): void => {
    this.patch({ editorError: null, editorErrorSlot: -1 });
  };

  private async runEditor(
    slot: number,
    request: (slot: number) => Promise<ControlPatchEditor>,
  ): Promise<boolean> {
    this.patch({ editorBusy: true, editorError: null, editorErrorSlot: -1 });

    try {
      const result = await request(slot);
      const current = this.context.getSnapshot().state;

      this.patch({
        phase: "ready",
        state:
          current === null ? null : withEditorOpen(current, slot, result.open),
        editorOpenSlot: result.open ? slot : -1,
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
        editorBusy: false,
        editorError: messageOf(error),
        editorErrorSlot: slot,
      });
      return false;
    }
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
