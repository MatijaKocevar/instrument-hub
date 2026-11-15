import type { ControlState, ControlTransportPatch } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";

export class TransportActions {
  private transportSequence = 0;
  private transportInFlight = 0;

  constructor(
    private readonly context: ActionContext,
    private readonly disarm: (
      slot: number | null,
      report?: boolean,
    ) => Promise<boolean>,
  ) {}

  updateTransport = async (
    patch: ControlTransportPatch,
    report = true,
  ): Promise<boolean> => {
    const previous = this.snapshot.state;
    const sequence = this.transportSequence + 1;
    this.transportSequence = sequence;
    this.transportInFlight += 1;

    if (previous !== null) {
      this.patch({
        state: { ...previous, transport: { ...previous.transport, ...patch } },
      });
    }

    try {
      const transport = await this.context.api.controlTransport(patch);

      if (sequence === this.transportSequence && this.snapshot.state !== null) {
        this.patch({
          state: { ...this.snapshot.state, transport },
        });
      }

      return true;
    } catch (error) {
      if (sequence === this.transportSequence) {
        this.patch({
          ...(previous === null ? {} : { state: previous }),
        });
      }

      if (report)
        this.notify(
          failureToast("Could not change transport", error, "transport"),
        );

      return false;
    } finally {
      this.transportInFlight -= 1;
    }
  };

  panic = async (): Promise<boolean> => {
    const disarmed = await this.disarm(null, false);
    const stopped = await this.updateTransport(
      {
        playing: false,
        ppq: 0,
        positionSamples: 0,
      },
      false,
    );

    if (!disarmed || !stopped) {
      this.notify({
        ...failureToast(
          "Panic failed",
          "The engine did not acknowledge the stop command.",
          "panic",
        ),
        sticky: true,
      });
      return false;
    }

    this.notify({
      severity: "success",
      title: "All sound stopped",
      key: "panic",
    });

    return true;
  };

  preservePending = (state: ControlState): ControlState => {
    const current = this.snapshot.state;

    return this.transportInFlight > 0 && current !== null
      ? { ...state, transport: current.transport }
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
