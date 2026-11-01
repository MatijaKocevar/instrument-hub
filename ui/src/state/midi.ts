import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

/**
    Read the MIDI inputs, toggle one and run the Launchpad pad test.

    Busy/error flags stay on the snapshot so the Audio page can render the
    request state next to the port list and the pad-test button.
*/
export class MidiActions
{
    constructor (private readonly context: ActionContext) {}

    /** Read the ALSA inputs and controller status behind `GET /midi`. */
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

    /** Open or close one MIDI input. */
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

    /** Light the Launchpad grid for the pad-test animation. */
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
