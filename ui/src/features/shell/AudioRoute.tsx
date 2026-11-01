import type { ReactElement } from "react";

import { AudioSettingsPanel } from "../audio-settings";
import { MidiPanel } from "../midi";
import type { ShellData } from "./useShellData";

/** Audio route: device settings, latency measurement and MIDI inputs. */
export function AudioRoute ({ shell }: { readonly shell: ShellData }): ReactElement
{
    const { app, state, store } = shell;

    return (
        <section className="app__stack" data-testid="audio-page">
            <AudioSettingsPanel
                audio={app.audio}
                measurement={app.measurement}
                error={app.audioError}
                measurementError={app.measurementError}
                busy={app.audioBusy}
                loading={state === null && app.audio === null}
                onChange={patch => { void store.updateAudio (patch); }}
                onMeasure={() => { void store.measureLatency (); }}
            />

            <MidiPanel
                midi={app.midi}
                error={app.midiError}
                busy={app.midiBusy}
                padTestBusy={app.padTestBusy}
                padTestError={app.padTestError}
                loading={app.midi === null}
                onToggleInput={(id, enabled) => {
                    void store.toggleMidiInput (id, enabled);
                }}
                onPadTest={() => { void store.runPadTest (); }}
            />
        </section>
    );
}
