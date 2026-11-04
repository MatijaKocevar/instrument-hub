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

  recommendAudioSetup = async (): Promise<boolean> => {
    this.patch({ audioWizardBusy: true, audioWizardError: null });

    try {
      const audioWizard = await this.context.api.recommendAudioSetup();

      this.patch({
        audioWizard,
        audioWizardBusy: false,
        audioWizardError: audioWizard.valid
          ? null
          : audioWizard.error.length > 0
            ? audioWizard.error
            : "no stable audio setting was found",
      });

      return audioWizard.valid;
    } catch (error) {
      this.patch({
        audioWizardBusy: false,
        audioWizardError: messageOf(error),
      });
      return false;
    }
  };

  applyAudioSetup = async (): Promise<boolean> => {
    this.patch({ audioWizardBusy: true, audioWizardError: null });

    try {
      const audio = await this.context.api.applyAudioSetup();

      this.patch({
        audio,
        audioWizard: null,
        audioWizardBusy: false,
        audioWizardError: null,
        measurement: null,
        measurementError: null,
      });

      return true;
    } catch (error) {
      this.patch({
        audioWizardBusy: false,
        audioWizardError: messageOf(error),
      });
      return false;
    }
  };

  dismissAudioWizard = (): void => {
    this.patch({ audioWizard: null, audioWizardError: null });
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
