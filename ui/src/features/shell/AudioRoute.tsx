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
        unavailable={app.audioError !== null}
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
        onRetry={() => {
          void store.refresh();
        }}
      />

      <MidiPanel
        midi={app.midi}
        unavailable={app.midiError !== null}
        busy={app.midiBusy}
        loading={app.midi === null}
        onChangeInput={(patch) => {
          void store.updateMidiInput(patch);
        }}
        onRefreshMidi={() => {
          void store.refreshMidi();
        }}
      />
    </section>
  );
}
