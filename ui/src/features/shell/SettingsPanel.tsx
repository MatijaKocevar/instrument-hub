import type { ReactElement } from "react";

import type { ControlSettings } from "../../api/client";

function formatGain(gain: number): string {
  return `${Math.round(gain * 100)}%`;
}

export function SettingsPanel({
  settings,
}: {
  readonly settings: ControlSettings;
}): ReactElement {
  return (
    <article className="panel">
      <h2 className="panel__title">Settings</h2>
      <dl className="panel__meta">
        <div>
          <dt>Master gain</dt>
          <dd>{formatGain(settings.masterGain)}</dd>
        </div>
        <div>
          <dt>Input gain</dt>
          <dd>{formatGain(settings.inputGain)}</dd>
        </div>
        <div>
          <dt>MIDI channel</dt>
          <dd>{settings.midiChannel}</dd>
        </div>
        <div>
          <dt>Metronome</dt>
          <dd>{settings.metronome ? "On" : "Off"}</dd>
        </div>
      </dl>
    </article>
  );
}
