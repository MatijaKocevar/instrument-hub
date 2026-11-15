import type { ControlPatchUpdate, ControlState } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";
import { withPatchUpdate } from "./patchModel";

export class PatchUpdateActions {
  private patchSequence = 0;
  private patchInFlight = 0;
  private pendingPatch: { slot: number; patch: ControlPatchUpdate } | null =
    null;

  constructor(private readonly context: ActionContext) {}

  updatePatch = async (
    slot: number,
    patch: ControlPatchUpdate,
  ): Promise<boolean> => {
    const previous = this.snapshot.state;

    if (previous === null) return false;

    const sequence = this.patchSequence + 1;
    this.patchSequence = sequence;
    this.patchInFlight += 1;
    this.pendingPatch = { slot, patch };

    this.patch({ state: withPatchUpdate(previous, slot, patch) });

    try {
      const state = await this.context.api.updatePatch(slot, patch);

      if (sequence === this.patchSequence) {
        this.patch({
          phase: "ready",
          state,
          failedAttempts: 0,
          updatedAt: this.context.now(),
        });
      }

      return true;
    } catch (error) {
      if (sequence === this.patchSequence) this.patch({ state: previous });

      this.notify(
        failureToast("Could not update patch", error, `patch:${String(slot)}`),
      );

      return false;
    } finally {
      this.patchInFlight -= 1;

      if (this.patchInFlight === 0 && sequence === this.patchSequence)
        this.pendingPatch = null;
    }
  };

  preservePending = (state: ControlState): ControlState => {
    if (this.patchInFlight === 0 || this.pendingPatch === null) return state;

    return withPatchUpdate(
      state,
      this.pendingPatch.slot,
      this.pendingPatch.patch,
    );
  };

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private get snapshot() {
    return this.context.getSnapshot();
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
