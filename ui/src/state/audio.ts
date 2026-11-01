import type { ControlAudioPatch } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Change the audio device settings and run the latency probe.

    The change response carries the post-change readout, so the panel updates
    from the same round trip that changed the engine.
*/
export class AudioActions
{
    constructor (private readonly context: ActionContext) {}

    /** Apply a buffer, sample-rate, device or channel-mapping change. */
    updateAudio = async (patch: ControlAudioPatch): Promise<boolean> =>
    {
        this.patch ({ audioBusy: true, audioError: null });

        try
        {
            const audio = await this.context.api.updateAudio (patch);

            this.patch ({
                audio,
                audioBusy: false,
                audioError: null,
                measurement: null,
                measurementError: null,
            });

            return true;
        }
        catch (error)
        {
            this.patch ({ audioBusy: false, audioError: messageOf (error) });
            return false;
        }
    };

    /** Run the latency probe and show its result next to the readout. */
    measureLatency = async (): Promise<boolean> =>
    {
        this.patch ({ measurementError: null });

        try
        {
            const measurement = await this.context.api.measureLatency ();

            this.patch ({ measurement, measurementError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ measurementError: messageOf (error) });
            return false;
        }
    };

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
