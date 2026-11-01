import type { ControlState } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";
import { withArmedSlot } from "./patchModel";

/**
    Arm and disarm the patch that plays.

    The highlight moves synchronously before the request leaves, so the grid
    reacts in the same tick as the click. A stale response never wins and a
    rejection restores the confirmed highlight.
*/
export class PatchArmActions
{
    /** Sequence of arm requests; a stale response must not win. */
    private armSequence = 0;
    /** Number of arm requests in flight, used to keep the highlight during polls. */
    private armInFlight = 0;

    constructor (private readonly context: ActionContext) {}

    /**
        Arm `slot` (or disarm with `null`).

        Returns `false` without a request when the slot is outside the engine's
        slot list.
    */
    armSlot = async (slot: number | null): Promise<boolean> =>
    {
        const target = slot === null ? -1 : slot;
        const previous = this.snapshot.state;

        if (previous !== null && target >= previous.slots.length)
            return false;

        const sequence = this.armSequence + 1;
        this.armSequence = sequence;
        this.armInFlight += 1;

        if (previous !== null)
            this.patch ({ state: withArmedSlot (previous, target), error: null });

        try
        {
            const state = await this.context.api.armSlot (slot);

            if (sequence === this.armSequence)
            {
                this.patch ({
                    phase: "ready",
                    state,
                    error: null,
                    failedAttempts: 0,
                    updatedAt: this.context.now(),
                });
            }

            return true;
        }
        catch (error)
        {
            if (sequence === this.armSequence)
            {
                this.patch ({
                    error: messageOf (error),
                    ...(previous === null ? {} : { state: previous }),
                });
            }

            return false;
        }
        finally
        {
            this.armInFlight -= 1;
        }
    };

    /**
        Keep the optimistic highlight while an arm request is in flight.

        A poll can land before the request answers; its snapshot still names
        the previous slot. Re-applying the confirmed snapshot's `armedSlot`
        keeps the UI from snapping back until the action confirms or rolls back.
    */
    preservePending = (state: ControlState): ControlState =>
    {
        const current = this.snapshot.state;

        return this.armInFlight > 0 && current !== null
            ? withArmedSlot (state, current.armedSlot)
            : state;
    };

    private get snapshot ()
    {
        return this.context.getSnapshot ();
    }

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
