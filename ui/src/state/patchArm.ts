import type { ControlState } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";
import { withArmedSlot } from "./patchModel";

export class PatchArmActions {
  private armSequence = 0;
  private armInFlight = 0;

  constructor(private readonly context: ActionContext) {}

  armSlot = async (slot: number | null, report = true): Promise<boolean> => {
    const target = slot === null ? -1 : slot;
    const previous = this.snapshot.state;

    if (previous !== null && target >= previous.slots.length) return false;

    const sequence = this.armSequence + 1;
    this.armSequence = sequence;
    this.armInFlight += 1;

    if (previous !== null)
      this.patch({ state: withArmedSlot(previous, target) });

    try {
      const state = await this.context.api.armSlot(slot);

      if (sequence === this.armSequence) {
        this.patch({
          phase: "ready",
          state,
          failedAttempts: 0,
          updatedAt: this.context.now(),
        });
      }

      return true;
    } catch (error) {
      if (sequence === this.armSequence) {
        this.patch({
          ...(previous === null ? {} : { state: previous }),
        });
      }

      if (report)
        this.notify(failureToast("Could not arm slot", error, "arm-slot"));

      return false;
    } finally {
      this.armInFlight -= 1;
    }
  };

  preservePending = (state: ControlState): ControlState => {
    const current = this.snapshot.state;

    return this.armInFlight > 0 && current !== null
      ? withArmedSlot(state, current.armedSlot)
      : state;
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
