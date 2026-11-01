import type { ControlPatchUpdate, ControlState } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";
import { withPatchUpdate } from "./patchModel";

export class PatchUpdateActions
{
    private patchSequence = 0;
    private patchInFlight = 0;
    private pendingPatch: { slot: number; patch: ControlPatchUpdate } | null = null;

    constructor (private readonly context: ActionContext) {}

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
