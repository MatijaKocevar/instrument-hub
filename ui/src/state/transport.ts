import type { ControlState, ControlTransportPatch } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Start, stop or reposition the transport, and panic the rig.

    The change is applied optimistically so Play/Pause reacts on the next
    frame rather than after the HTTP round trip; a rejected request restores
    the previous transport. Panic composes the injected disarm with a stop.
*/
export class TransportActions
{
    /** Sequence of transport requests; a stale response must not win. */
    private transportSequence = 0;
    /** Number of transport requests in flight, used to keep Play/Pause during polls. */
    private transportInFlight = 0;

    constructor (private readonly context: ActionContext,
                 private readonly disarm: (slot: number | null) => Promise<boolean>) {}

    updateTransport = async (patch: ControlTransportPatch): Promise<boolean> =>
    {
        const previous = this.snapshot.state;
        const sequence = this.transportSequence + 1;
        this.transportSequence = sequence;
        this.transportInFlight += 1;

        if (previous !== null)
        {
            this.patch ({
                state: { ...previous, transport: { ...previous.transport, ...patch } },
                error: null,
            });
        }

        try
        {
            const transport = await this.context.api.controlTransport (patch);

            if (sequence === this.transportSequence && this.snapshot.state !== null)
            {
                this.patch ({
                    state: { ...this.snapshot.state, transport },
                    error: null,
                });
            }

            return true;
        }
        catch (error)
        {
            if (sequence === this.transportSequence)
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
            this.transportInFlight -= 1;
        }
    };

    /**
        Silence the rig by stopping the transport and disarming every slot.

        Both requests are attempted even if one fails, because a stuck note is
        worse than a stale status line.
    */
    panic = async (): Promise<boolean> =>
    {
        const disarmed = await this.disarm (null);
        const stopped = await this.updateTransport ({
            playing: false,
            ppq: 0,
            positionSamples: 0,
        });

        return disarmed && stopped;
    };

    /**
        Keep the optimistic transport while a request is in flight.

        A poll can land before the request answers; its snapshot still carries
        the previous transport. Re-applying the current one keeps Play/Pause
        from snapping back until the action confirms or rolls back.
    */
    preservePending = (state: ControlState): ControlState =>
    {
        const current = this.snapshot.state;

        return this.transportInFlight > 0 && current !== null
            ? { ...state, transport: current.transport }
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
