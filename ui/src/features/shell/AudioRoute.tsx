import type { ReactElement } from "react";

import { AudioSettingsPanel } from "../audio-settings";
import { MidiPanel } from "../midi";
import type { ShellData } from "./useShellData";

export function AudioRoute({
  shell,
}: {
  readonly shell: ShellData;
}): ReactElement {
  const { app, state, store } = shell;

  return (
    <section className="app__stack" data-testid="audio-page">
      <AudioSettingsPanel
        audio={app.audio}
        measurement={app.measurement}
        wizard={app.audioWizard}
        error={app.audioError}
        measurementError={app.measurementError}
        wizardError={app.audioWizardError}
        busy={app.audioBusy}
        wizardBusy={app.audioWizardBusy}
        loading={state === null && app.audio === null}
        onChange={(patch) => {
          void store.updateAudio(patch);
        }}
        onMeasure={(source) => {
          void store.measureLatency(source);
        }}
        onRecommend={() => {
          void store.recommendAudioSetup();
        }}
        onApplySetup={() => {
          void store.applyAudioSetup();
        }}
        onDismissSetup={() => {
          store.dismissAudioWizard();
        }}
      />

      <MidiPanel
        midi={app.midi}
        error={app.midiError}
        busy={app.midiBusy}
        padTestBusy={app.padTestBusy}
        padTestError={app.padTestError}
        loading={app.midi === null}
        onToggleInput={(id, enabled) => {
          void store.toggleMidiInput(id, enabled);
        }}
        onPadTest={() => {
          void store.runPadTest();
        }}
      />
    </section>
  );
}
