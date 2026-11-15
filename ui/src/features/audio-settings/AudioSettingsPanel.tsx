import { useState, type ReactElement, type ReactNode } from "react";

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
import { AudioSettingsTabs, type AudioSettingsTab } from "./AudioSettingsTabs";
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
  readonly midiPanel?: ReactNode;
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
  midiPanel = null,
}: AudioSettingsPanelProps): ReactElement {
  const [tab, setTab] = useState<AudioSettingsTab>("device");

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

      <AudioSettingsTabs tab={tab} onChange={setTab} />

      {tab === "device" ? (
        <>
          <p className="audio-settings__lede">
            Pick the interface you play through, then set the sample rate and
            buffer. Smaller buffers mean lower latency; raise the buffer again
            if audio starts crackling.
          </p>

          <RestoreNotice audio={audio} busy={busy} onChange={onChange} />

          <div className="audio-settings__layout">
            <section className="audio-settings__panel audio-settings__panel--devices">
              <h3 className="audio-settings__panel-title">Interface</h3>
              <DevicePicker audio={audio} busy={busy} onChange={onChange} />
            </section>

            <div className="audio-settings__column">
              <section className="audio-settings__panel">
                <h3 className="audio-settings__panel-title">Format</h3>
                <DeviceControls audio={audio} busy={busy} onChange={onChange} />
              </section>

              <section className="audio-settings__panel">
                <h3 className="audio-settings__panel-title">Latency</h3>
                <LatencyReadout latency={audio.latency} />
              </section>

              <section className="audio-settings__panel">
                <h3 className="audio-settings__panel-title">Diagnostics</h3>
                <div className="audio-settings__readout-row">
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
              </section>

              <AudioSetupControl
                wizard={wizard}
                busy={wizardBusy}
                onRecommend={onRecommend}
                onApply={onApplySetup}
                onDismiss={onDismissSetup}
              />
            </div>
          </div>
        </>
      ) : null}

      {tab === "channels" ? (
        <ChannelMaps
          inputChannels={audio.inputChannels}
          outputChannels={audio.outputChannels}
          onChange={onChange}
        />
      ) : null}

      {tab === "midi" ? midiPanel : null}

      <AudioSettingsFooter audio={audio} />
    </section>
  );
}
