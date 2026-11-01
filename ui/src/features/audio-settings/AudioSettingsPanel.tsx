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
import { XrunCounter } from "./XrunCounter";
import "./audio-settings.css";

export interface AudioSettingsPanelProps
{
    /** Latest `GET /audio`, or null before the first successful request. */
    readonly audio: ControlAudioState | null;
    /** Latest latency probe result. */
    readonly measurement?: ControlAudioMeasurement | null;
    /** Message from a failed `GET/POST /audio`. */
    readonly error?: string | null;
    /** Message from a failed probe request. */
    readonly measurementError?: string | null;
    /** True while a settings request is in flight. */
    readonly busy?: boolean;
    /** Show a skeleton while the first audio state arrives. */
    readonly loading?: boolean;
    /** Called with a partial patch for every control change. */
    readonly onChange?: (patch: ControlAudioPatch) => void;
    /** Called by the Measure button. */
    readonly onMeasure?: () => void;
}

/**
    The audio device panel.

    Device selection, channel maps, latency, xruns and the loopback
    measurement each own a component; this file is the layout and the
    loading state.
*/
export function AudioSettingsPanel ({
    audio,
    measurement = null,
    error = null,
    measurementError = null,
    busy = false,
    loading = false,
    onChange,
    onMeasure,
}: AudioSettingsPanelProps): ReactElement
{
    if (loading || audio === null)
    {
        return (
            <section
                className="audio-settings"
                aria-label="Audio settings"
                aria-busy="true"
                data-testid="audio-settings"
            >
                <AudioSettingsHeader audio={null} />
                <div className="audio-settings__loading" data-testid="audio-settings-loading">
                    {error !== null && error.length > 0
                        ? <span className="audio-settings__error" data-testid="audio-error">{error}</span>
                        : "Waiting for the audio device…"}
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

            {error !== null && error.length > 0
                ? <p className="audio-settings__error" data-testid="audio-error">{error}</p>
                : null}

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
