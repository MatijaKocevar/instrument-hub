import type { ControlAudioPatch } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";

export class AudioActions {
  constructor(private readonly context: ActionContext) {}

  updateAudio = async (patch: ControlAudioPatch): Promise<boolean> => {
    this.patch({ audioBusy: true });

    try {
      const audio = await this.context.api.updateAudio(patch);

      this.patch({
        audio,
        audioBusy: false,
        measurement: null,
      });

      return true;
    } catch (error) {
      this.patch({ audioBusy: false });
      this.notify(
        failureToast("Could not change audio settings", error, "audio-update"),
      );
      return false;
    }
  };

  recommendAudioSetup = async (): Promise<boolean> => {
    this.patch({ audioWizardBusy: true });

    try {
      const audioWizard = await this.context.api.recommendAudioSetup();

      if (audioWizard.valid) {
        this.patch({ audioWizard, audioWizardBusy: false });
        return true;
      }

      this.patch({ audioWizard: null, audioWizardBusy: false });
      this.notify({
        severity: "warning",
        title: "No stable audio setting found",
        detail:
          audioWizard.error.length > 0
            ? audioWizard.error
            : "Every tested rate and buffer produced xruns.",
        key: "audio-recommend",
      });

      return false;
    } catch (error) {
      this.patch({ audioWizard: null, audioWizardBusy: false });
      this.notify(
        failureToast(
          "Could not test the audio setup",
          error,
          "audio-recommend",
        ),
      );
      return false;
    }
  };

  applyAudioSetup = async (): Promise<boolean> => {
    this.patch({ audioWizardBusy: true });

    try {
      const audio = await this.context.api.applyAudioSetup();

      this.patch({
        audio,
        audioWizard: null,
        audioWizardBusy: false,
        measurement: null,
      });

      this.notify({
        severity: "success",
        title: "Audio setup applied",
        detail: `${audio.deviceName} · ${String(audio.sampleRate)} Hz / ${String(audio.bufferSize)} frames`,
        key: "audio-apply",
      });

      return true;
    } catch (error) {
      this.patch({ audioWizardBusy: false });
      this.notify(
        failureToast("Could not apply the audio setup", error, "audio-apply"),
      );
      return false;
    }
  };

  dismissAudioWizard = (): void => {
    this.patch({ audioWizard: null });
  };

  measureLatency = async (source: "loopback" | "device"): Promise<boolean> => {
    try {
      const measurement = await this.context.api.measureLatency({ source });

      this.patch({ measurement });

      if (measurement.valid) {
        this.notify({
          severity: "success",
          title: "Latency measured",
          detail: `${measurement.roundTripMs.toFixed(2)} ms round trip`,
          key: "audio-measure",
        });
        return true;
      }

      this.notify({
        severity: "warning",
        title: "Latency measurement failed",
        detail:
          measurement.error.length > 0
            ? measurement.error
            : "No loopback signal was detected.",
        key: "audio-measure",
      });

      return false;
    } catch (error) {
      this.patch({ measurement: null });
      this.notify(
        failureToast("Could not measure latency", error, "audio-measure"),
      );
      return false;
    }
  };

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
