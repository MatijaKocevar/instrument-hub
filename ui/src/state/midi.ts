import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class MidiActions
{
    constructor (private readonly context: ActionContext) {}

    loadMidi = async (): Promise<boolean> =>
    {
        this.patch ({ midiBusy: true, midiError: null });

        try
        {
            const midi = await this.context.api.getMidi ();

            this.patch ({ midi, midiBusy: false, midiError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ midiBusy: false, midiError: messageOf (error) });
            return false;
        }
    };

    toggleMidiInput = async (id: string, enabled: boolean): Promise<boolean> =>
    {
        this.patch ({ midiBusy: true, midiError: null });

        try
        {
            const midi = await this.context.api.setMidiInput (id, enabled);

            this.patch ({ midi, midiBusy: false, midiError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ midiBusy: false, midiError: messageOf (error) });
            return false;
        }
    };

    runPadTest = async (): Promise<boolean> =>
    {
        this.patch ({ padTestBusy: true, padTestError: null });

        try
        {
            const midi = await this.context.api.runMidiPadTest ();

            this.patch ({ midi, padTestBusy: false, padTestError: null });
            return true;
        }
        catch (error)
        {
            this.patch ({ padTestBusy: false, padTestError: messageOf (error) });
            return false;
        }
    };

    private patch (change: Parameters<ActionContext["patch"]>[0]): void
    {
        this.context.patch (change);
    }
}
