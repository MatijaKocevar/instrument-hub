import type { ReactElement } from "react";

import type {
  ControlAudioMeasurement,
  ControlAudioPatch,
  ControlAudioState,
  ControlAudioWizard,
} from "../../api/client";
import type { MeasurementSource } from "../../api/audio";
import { AudioSettingsFooter } from "./AudioSettingsFooter";
import { AudioSettingsHeader } from "./AudioSettingsHeader";
import { AudioSettingsSkeleton } from "./AudioSettingsSkeleton";
import { AudioSetupControl } from "./AudioSetupControl";
import { ChannelMaps } from "./ChannelMaps";
import { DeviceControls } from "./DeviceControls";
import { DevicePicker } from "./DevicePicker";
import { LatencyReadout } from "./LatencyReadout";
import { MeasureControl } from "./MeasureControl";
import { RestoreNotice } from "./RestoreNotice";
import { XrunCounter } from "./XrunCounter";
import "./audio-settings.css";

export interface AudioSettingsPanelProps {
  readonly audio: ControlAudioState | null;
  readonly measurement?: ControlAudioMeasurement | null;
  readonly wizard?: ControlAudioWizard | null;
  readonly error?: string | null;
  readonly measurementError?: string | null;
  readonly wizardError?: string | null;
  readonly busy?: boolean;
  readonly wizardBusy?: boolean;
  readonly loading?: boolean;
  readonly onChange?: (patch: ControlAudioPatch) => void;
  readonly onMeasure?: (source: MeasurementSource) => void;
  readonly onRecommend?: () => void;
  readonly onApplySetup?: () => void;
  readonly onDismissSetup?: () => void;
}

export function AudioSettingsPanel({
  audio,
  measurement = null,
  wizard = null,
  error = null,
  measurementError = null,
  wizardError = null,
  busy = false,
  wizardBusy = false,
  loading = false,
  onChange,
  onMeasure,
  onRecommend,
  onApplySetup,
  onDismissSetup,
}: AudioSettingsPanelProps): ReactElement {
  if (loading || audio === null) return <AudioSettingsSkeleton error={error} />;

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

      <DevicePicker audio={audio} busy={busy} onChange={onChange} />

      <DeviceControls audio={audio} busy={busy} onChange={onChange} />

      <AudioSetupControl
        wizard={wizard}
        busy={wizardBusy}
        error={wizardError}
        onRecommend={onRecommend}
        onApply={onApplySetup}
        onDismiss={onDismissSetup}
      />

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
