import type { ControlPatchEditor } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";
import { withEditorOpen } from "./patchModel";

/**
    Open and close one patch's native plugin editor.

    Only these explicit actions create a window; selecting or arming a patch
    never does. The engine answers with `{ slot, open, reused }`, and the
    page's patch is updated in place so the button flips immediately.
*/
export class PatchEditorActions
{
    constructor (private readonly context: ActionContext) {}

    /** Open (or focus) the patch's native plugin editor. */
    openInstrumentEditor = (slot: number): Promise<boolean> =>
        this.runEditor (slot, slotToOpen => this.context.api.openPatchEditor (slotToOpen));

    /** Close the patch's native editor. Idempotent. */
    closeInstrumentEditor = (slot: number): Promise<boolean> =>
        this.runEditor (slot, slotToClose => this.context.api.closePatchEditor (slotToClose));

    private async runEditor (slot: number,
                             request: (slot: number) => Promise<ControlPatchEditor>): Promise<boolean>
    {
        this.patch ({ editorBusy: true, editorError: null });

        try
        {
            const result = await request (slot);
            const current = this.context.getSnapshot ().state;

            this.patch ({
                phase: "ready",
                state: current === null ? null : withEditorOpen (current, slot, result.open),
                editorOpenSlot: result.open ? slot : -1,
                editorBusy: false,
                editorError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.context.now(),
            });

            return true;
        }
        catch (error)
        {
            this.patch ({ editorBusy: false, editorError: messageOf (error) });
            return false;
        }
    }

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
