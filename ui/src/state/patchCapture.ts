import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Save the running plugin's sound into its patch.

    This is the explicit counterpart to the automatic capture on switch,
    editor close and eviction: a tweak can be kept without changing patches.
    The engine writes the sidecar blob before it answers, so the confirmation
    already describes persistent state.
*/
export class PatchCaptureActions
{
    constructor (private readonly context: ActionContext) {}

    savePatchSound = async (slot: number): Promise<boolean> =>
    {
        this.patch ({
            captureBusy: true,
            captureSlot: -1,
            capture: null,
            captureError: null,
        });

        try
        {
            const capture = await this.context.api.capturePatchState (slot);

            this.patch ({
                phase: "ready",
                captureBusy: false,
                captureSlot: slot,
                capture,
                captureError: null,
                error: null,
                failedAttempts: 0,
                updatedAt: this.context.now(),
            });

            return true;
        }
        catch (error)
        {
            this.patch ({
                captureBusy: false,
                captureSlot: slot,
                capture: null,
                captureError: messageOf (error),
            });

            return false;
        }
    };

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
