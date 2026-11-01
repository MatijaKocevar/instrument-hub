import type { ControlPatchUpdate, ControlState } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";
import { withPatchUpdate } from "./patchModel";

/**
    Edit a patch's mix and colour.

    The change is applied optimistically (so a cell recolours or a fader moves
    on the next render) and the engine's complete state is adopted when the
    request answers. A rejection restores the last confirmed state.
*/
export class PatchUpdateActions
{
    /** Sequence of patch requests; a stale response must not win. */
    private patchSequence = 0;
    /** Number of patch requests in flight, used to keep the fader during polls. */
    private patchInFlight = 0;
    /** Latest optimistic patch while a request is in flight. */
    private pendingPatch: { slot: number; patch: ControlPatchUpdate } | null = null;

    constructor (private readonly context: ActionContext) {}

    /** Change one patch's mix or colour. */
    updatePatch = async (slot: number, patch: ControlPatchUpdate): Promise<boolean> =>
    {
        const previous = this.snapshot.state;

        if (previous === null)
            return false;

        const sequence = this.patchSequence + 1;
        this.patchSequence = sequence;
        this.patchInFlight += 1;
        this.pendingPatch = { slot, patch };

        this.patch ({ state: withPatchUpdate (previous, slot, patch), error: null });

        try
        {
            const state = await this.context.api.updatePatch (slot, patch);

            if (sequence === this.patchSequence)
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
            if (sequence === this.patchSequence)
                this.patch ({ error: messageOf (error), state: previous });

            return false;
        }
        finally
        {
            this.patchInFlight -= 1;

            if (this.patchInFlight === 0 && sequence === this.patchSequence)
                this.pendingPatch = null;
        }
    };

    /**
        Recolour every patch in a family.

        The API stores colour per patch, so the family default is applied as
        one PATCH per patch. Sequential requests keep the final state
        deterministic if one is rejected.
    */
    updateFamilyColour = async (familyId: string, colorIndex: number): Promise<boolean> =>
    {
        const family = this.snapshot.state?.families.find (candidate => candidate.id === familyId);

        if (family === undefined)
            return false;

        for (const patch of family.patches)
        {
            if (! await this.updatePatch (patch.slot, { colorIndex }))
                return false;
        }

        return true;
    };

    /**
        Keep the optimistic patch while a request is in flight.

        A poll can land before the PATCH answers; its snapshot still carries
        the previous value. Re-applying the latest optimistic field keeps the
        UI from snapping back until the action confirms or rolls back.
    */
    preservePending = (state: ControlState): ControlState =>
    {
        if (this.patchInFlight === 0 || this.pendingPatch === null)
            return state;

        return withPatchUpdate (state, this.pendingPatch.slot, this.pendingPatch.patch);
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
