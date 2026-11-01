import type { ControlState, ControlTransportPatch } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class TransportActions {
  private transportSequence = 0;
  private transportInFlight = 0;

  constructor(
    private readonly context: ActionContext,
    private readonly disarm: (slot: number | null) => Promise<boolean>,
  ) {}

  updateTransport = async (patch: ControlTransportPatch): Promise<boolean> => {
    const previous = this.snapshot.state;
    const sequence = this.transportSequence + 1;
    this.transportSequence = sequence;
    this.transportInFlight += 1;

    if (previous !== null) {
      this.patch({
        state: { ...previous, transport: { ...previous.transport, ...patch } },
        error: null,
      });
    }

    try {
      const transport = await this.context.api.controlTransport(patch);

      if (sequence === this.transportSequence && this.snapshot.state !== null) {
        this.patch({
          state: { ...this.snapshot.state, transport },
          error: null,
        });
      }

      return true;
    } catch (error) {
      if (sequence === this.transportSequence) {
        this.patch({
          error: messageOf(error),
          ...(previous === null ? {} : { state: previous }),
        });
      }

      return false;
    } finally {
      this.transportInFlight -= 1;
    }
  };

  panic = async (): Promise<boolean> => {
    const disarmed = await this.disarm(null);
    const stopped = await this.updateTransport({
      playing: false,
      ppq: 0,
      positionSamples: 0,
    });

    return disarmed && stopped;
  };

  preservePending = (state: ControlState): ControlState => {
    const current = this.snapshot.state;

    return this.transportInFlight > 0 && current !== null
      ? { ...state, transport: current.transport }
      : state;
  };

  private get snapshot() {
    return this.context.getSnapshot();
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
