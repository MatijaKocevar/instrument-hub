import type { ControlAudioPatch } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class AudioActions {
  constructor(private readonly context: ActionContext) {}

  updateAudio = async (patch: ControlAudioPatch): Promise<boolean> => {
    this.patch({ audioBusy: true, audioError: null });

    try {
      const audio = await this.context.api.updateAudio(patch);

      this.patch({
        audio,
        audioBusy: false,
        audioError: null,
        measurement: null,
        measurementError: null,
      });

      return true;
    } catch (error) {
      this.patch({ audioBusy: false, audioError: messageOf(error) });
      return false;
    }
  };

  measureLatency = async (source: "loopback" | "device"): Promise<boolean> => {
    this.patch({ measurementError: null });

    try {
      const measurement = await this.context.api.measureLatency({ source });

      this.patch({ measurement, measurementError: null });
      return true;
    } catch (error) {
      this.patch({ measurementError: messageOf(error) });
      return false;
    }
  };

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
