import type { ControlMidiInputUpdate } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast, messageOf } from "./errors";

export class MidiActions {
  constructor(private readonly context: ActionContext) {}

  loadMidi = async (): Promise<boolean> => {
    this.patch({ midiBusy: true, midiError: null });

    try {
      const midi = await this.context.api.getMidi();

      this.patch({ midi, midiBusy: false, midiError: null });
      return true;
    } catch (error) {
      this.patch({ midiBusy: false, midiError: messageOf(error) });
      return false;
    }
  };

  refreshMidi = async (): Promise<boolean> => {
    try {
      const midi = await this.context.api.getMidi();

      this.patch({ midi, midiError: null });
      return true;
    } catch (error) {
      this.patch({ midiError: messageOf(error) });
      return false;
    }
  };

  updateMidiInput = async (patch: ControlMidiInputUpdate): Promise<boolean> => {
    this.patch({ midiBusy: true, midiError: null });

    try {
      const midi = await this.context.api.updateMidiInput(patch);

      this.patch({ midi, midiBusy: false, midiError: null });
      return true;
    } catch (error) {
      this.patch({ midiBusy: false, midiError: messageOf(error) });
      this.notify(
        failureToast("Could not update MIDI input", error, "midi-input"),
      );
      return false;
    }
  };

  toggleMidiInput = async (id: string, enabled: boolean): Promise<boolean> => {
    return this.updateMidiInput({ id, enabled });
  };

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
