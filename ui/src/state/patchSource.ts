import type { ControlPatchSourceRequest } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Swap the instrument source behind one patch.

    The engine re-binds the slot to a freshly instantiated source, so the
    complete state is adopted and the chain is re-read for the new latency
    budget. The caller supplies the chain reload so this module stays free of
    FX knowledge.
*/
export class PatchSourceActions
{
    constructor (private readonly context: ActionContext,
                 private readonly reloadChain: (slot: number) => Promise<boolean>) {}

    changeInstrumentSource = async (slot: number,
                                    source: ControlPatchSourceRequest): Promise<boolean> =>
    {
        this.patch ({ sourceBusy: true, error: null });

        try
        {
            const state = await this.context.api.changePatchSource (slot, source);

            this.patch ({
                phase: "ready",
                state,
                sourceBusy: false,
                editorOpenSlot: -1,
                editorError: null,
                captureBusy: false,
                captureSlot: -1,
                capture: null,
                captureError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.context.now(),
            });

            await this.reloadChain (slot);
            return true;
        }
        catch (error)
        {
            this.patch ({ sourceBusy: false, error: messageOf (error) });
            return false;
        }
    };

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
