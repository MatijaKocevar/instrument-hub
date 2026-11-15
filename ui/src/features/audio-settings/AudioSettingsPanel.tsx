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
import { AudioSettingsUnavailable } from "./AudioSettingsUnavailable";
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
  readonly unavailable?: boolean;
  readonly busy?: boolean;
  readonly wizardBusy?: boolean;
  readonly loading?: boolean;
  readonly onChange?: (patch: ControlAudioPatch) => void;
  readonly onMeasure?: (source: MeasurementSource) => void;
  readonly onRecommend?: () => void;
  readonly onApplySetup?: () => void;
  readonly onDismissSetup?: () => void;
  readonly onRetry?: () => void;
}

export function AudioSettingsPanel({
  audio,
  measurement = null,
  wizard = null,
  unavailable = false,
  busy = false,
  wizardBusy = false,
  loading = false,
  onChange,
  onMeasure,
  onRecommend,
  onApplySetup,
  onDismissSetup,
  onRetry,
}: AudioSettingsPanelProps): ReactElement {
  if (loading || audio === null) {
    return unavailable ? (
      <AudioSettingsUnavailable onRetry={onRetry} />
    ) : (
      <AudioSettingsSkeleton />
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

      <RestoreNotice audio={audio} busy={busy} onChange={onChange} />

      <DevicePicker audio={audio} busy={busy} onChange={onChange} />

      <DeviceControls audio={audio} busy={busy} onChange={onChange} />

      <AudioSetupControl
        wizard={wizard}
        busy={wizardBusy}
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
          busy={busy}
          onMeasure={onMeasure}
        />
      </div>

      <AudioSettingsFooter audio={audio} />
    </section>
  );
}
