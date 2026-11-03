import type { ReactElement } from "react";

import type {
  ControlAudioMeasurement,
  ControlAudioPatch,
  ControlAudioState,
} from "../../api/client";
import { AudioSettingsFooter } from "./AudioSettingsFooter";
import { AudioSettingsHeader } from "./AudioSettingsHeader";
import { ChannelMaps } from "./ChannelMaps";
import { DeviceControls } from "./DeviceControls";
import { LatencyReadout } from "./LatencyReadout";
import { MeasureControl } from "./MeasureControl";
import { RestoreNotice } from "./RestoreNotice";
import { XrunCounter } from "./XrunCounter";
import "./audio-settings.css";

export interface AudioSettingsPanelProps {
  readonly audio: ControlAudioState | null;
  readonly measurement?: ControlAudioMeasurement | null;
  readonly error?: string | null;
  readonly measurementError?: string | null;
  readonly busy?: boolean;
  readonly loading?: boolean;
  readonly onChange?: (patch: ControlAudioPatch) => void;
  readonly onMeasure?: () => void;
}

export function AudioSettingsPanel({
  audio,
  measurement = null,
  error = null,
  measurementError = null,
  busy = false,
  loading = false,
  onChange,
  onMeasure,
}: AudioSettingsPanelProps): ReactElement {
  if (loading || audio === null) {
    return (
      <section
        className="audio-settings"
        aria-label="Audio settings"
        aria-busy="true"
        data-testid="audio-settings"
      >
        <AudioSettingsHeader audio={null} />
        <div
          className="audio-settings__loading"
          data-testid="audio-settings-loading"
        >
          {error !== null && error.length > 0 ? (
            <span className="audio-settings__error" data-testid="audio-error">
              {error}
            </span>
          ) : (
            "Waiting for the audio device…"
          )}
        </div>
      </section>
    );
  }

  return (
    <section
      className="audio-settings"
      aria-label="Audio settings"
      aria-busy={busy}
      data-testid="audio-settings"
      data-open={audio.open}
      data-buffer-size={audio.bufferSize}
    >
      <AudioSettingsHeader audio={audio} />

      {error !== null && error.length > 0 ? (
        <p className="audio-settings__error" data-testid="audio-error">
          {error}
        </p>
      ) : null}

      <RestoreNotice audio={audio} busy={busy} onChange={onChange} />

      <DeviceControls audio={audio} busy={busy} onChange={onChange} />

      <ChannelMaps
        inputChannels={audio.inputChannels}
        outputChannels={audio.outputChannels}
        onChange={onChange}
      />

      <div className="audio-settings__readout">
        <LatencyReadout latency={audio.latency} />
        <XrunCounter
          xruns={audio.xruns}
          deviceXruns={audio.deviceXruns}
          callbackOverruns={audio.callbackOverruns}
        />
        <MeasureControl
          measurement={measurement}
          error={measurementError}
          busy={busy}
          onMeasure={onMeasure}
        />
      </div>

      <AudioSettingsFooter audio={audio} />
    </section>
  );
}
