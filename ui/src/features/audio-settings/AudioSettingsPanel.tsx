import type { ChangeEvent, ReactElement } from "react";

import type {
    ControlAudioMeasurement,
    ControlAudioPatch,
    ControlAudioState,
} from "../../api/client";
import {
    activeChannels,
    channelSummary,
    deviceLabel,
    formatMs,
    formatSampleRate,
    toggleChannel,
} from "./model";
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

function uniqueSorted (values: number[], current: number): number[]
{
    const numbers = values.filter (value => Number.isFinite (value));

    if (!numbers.includes (current))
        numbers.push (current);

    return [...new Set (numbers)].sort ((a, b) => a - b);
}

function ChannelMap ({
    title,
    side,
    channels,
    onChange,
}: {
    title: string;
    side: "input" | "output";
    channels: ControlAudioState["inputChannels"];
    onChange?: ((patch: ControlAudioPatch) => void) | undefined;
}): ReactElement
{
    return (
        <fieldset className="audio-settings__channels" data-testid={`${side}-channels`}>
            <legend>
                {title}
                <span className="audio-settings__channel-summary">
                    {channelSummary (channels)}
                </span>
            </legend>

            <div className="audio-settings__channel-list">
                {channels.length === 0
                    ? <span className="audio-settings__muted">No channels</span>
                    : channels.map (channel => (
                          <label
                              className="audio-settings__channel"
                              key={`${side}-${String (channel.index)}`}
                          >
                              <input
                                  type="checkbox"
                                  data-testid={`${side}-channel`}
                                  data-index={channel.index}
                                  checked={channel.active}
                                  onChange={() => {
                                      if (onChange === undefined)
                                          return;

                                      onChange (side === "input"
                                          ? { inputChannels: toggleChannel (channels, channel.index) }
                                          : { outputChannels: toggleChannel (channels, channel.index) });
                                  }}
                              />
                              <span>{channel.name}</span>
                              <code>{channel.index}</code>
                          </label>
                      ))}
            </div>
        </fieldset>
    );
}

function Measurement ({ measurement, error }: {
    measurement: ControlAudioMeasurement | null | undefined;
    error: string | null | undefined;
}): ReactElement
{
    if (error !== null && error !== undefined && error.length > 0)
    {
        return (
            <span className="audio-settings__measure-error" data-testid="measurement-error">
                {error}
            </span>
        );
    }

    if (measurement === null || measurement === undefined)
    {
        return (
            <span
                className="audio-settings__muted"
                data-testid="measure-result"
                data-valid="none"
            >
                Not measured yet
            </span>
        );
    }

    if (! measurement.valid)
    {
        return (
            <span
                className="audio-settings__measure-error"
                data-testid="measure-result"
                data-valid="false"
            >
                {measurement.error.length > 0 ? measurement.error : "Measurement failed"}
            </span>
        );
    }

    return (
        <span className="audio-settings__measure-result" data-testid="measure-result" data-valid="true">
            <strong>{formatMs (measurement.roundTripMs)} ms</strong>
            <span>
                {measurement.roundTripSamples} samples · expected {formatMs (measurement.expectedMs)} ms
                · confidence {Math.round (measurement.confidence * 100)}%
            </span>
        </span>
    );
}

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
                <header className="audio-settings__header">
                    <h2 className="audio-settings__title">Audio settings</h2>
                    <span className="audio-settings__status" data-testid="audio-status">
                        Loading
                    </span>
                </header>
                <div className="audio-settings__loading" data-testid="audio-settings-loading">
                    {error !== null && error.length > 0
                        ? <span className="audio-settings__error" data-testid="audio-error">{error}</span>
                        : "Waiting for the audio device…"}
                </div>
            </section>
        );
    }

    const devicesOfType = audio.devices.filter (device => device.type === audio.deviceType);
    const deviceOptions = devicesOfType.length > 0 ? devicesOfType : audio.devices;
    const sampleRates = uniqueSorted (audio.sampleRates, audio.sampleRate);
    const bufferSizes = uniqueSorted (audio.bufferSizes, audio.bufferSize);
    const disabled = busy || onChange === undefined;

    const emit = (patch: ControlAudioPatch): void =>
    {
        onChange?.(patch);
    };

    return (
        <section
            className="audio-settings"
            aria-label="Audio settings"
            aria-busy={busy}
            data-testid="audio-settings"
            data-open={audio.open}
            data-buffer-size={audio.bufferSize}
        >
            <header className="audio-settings__header">
                <div>
                    <p className="audio-settings__eyebrow">Engine</p>
                    <h2 className="audio-settings__title">Audio settings</h2>
                </div>
                <span
                    className={`audio-settings__status audio-settings__status--${audio.open ? "open" : "closed"}`}
                    data-testid="audio-status"
                    data-open={audio.open}
                >
                    {audio.open ? `Open · ${deviceLabel (audio.deviceType, audio.deviceName)}` : "Device closed"}
                </span>
            </header>

            {error !== null && error.length > 0
                ? <p className="audio-settings__error" data-testid="audio-error">{error}</p>
                : null}

            <div className="audio-settings__controls">
                <label className="audio-settings__control">
                    <span>Device type</span>
                    <select
                        data-testid="device-type-select"
                        value={audio.deviceType}
                        disabled={disabled}
                        onChange={(event: ChangeEvent<HTMLSelectElement>) =>
                        {
                            emit ({ deviceType: event.target.value });
                        }}
                    >
                        {audio.deviceTypes.map (type => (
                            <option value={type} key={type}>{type}</option>
                        ))}
                    </select>
                </label>

                <label className="audio-settings__control">
                    <span>Device</span>
                    <select
                        data-testid="device-select"
                        value={audio.deviceName}
                        disabled={disabled}
                        onChange={(event: ChangeEvent<HTMLSelectElement>) =>
                        {
                            emit ({ deviceName: event.target.value });
                        }}
                    >
                        {deviceOptions.map (device => (
                            <option value={device.name} key={`${device.type}-${device.name}`}>
                                {device.name}
                            </option>
                        ))}
                    </select>
                </label>

                <label className="audio-settings__control">
                    <span>Sample rate</span>
                    <select
                        data-testid="sample-rate-select"
                        value={audio.sampleRate}
                        disabled={disabled}
                        onChange={(event: ChangeEvent<HTMLSelectElement>) =>
                        {
                            emit ({ sampleRate: Number (event.target.value) });
                        }}
                    >
                        {sampleRates.map (rate => (
                            <option value={rate} key={rate}>
                                {formatSampleRate (rate)}
                            </option>
                        ))}
                    </select>
                </label>

                <label className="audio-settings__control">
                    <span>Buffer size</span>
                    <select
                        data-testid="buffer-size-select"
                        value={audio.bufferSize}
                        disabled={disabled}
                        onChange={(event: ChangeEvent<HTMLSelectElement>) =>
                        {
                            emit ({ bufferSize: Number (event.target.value) });
                        }}
                    >
                        {bufferSizes.map (size => (
                            <option value={size} key={size}>
                                {size} frames
                            </option>
                        ))}
                    </select>
                </label>
            </div>

            <div className="audio-settings__maps">
                <ChannelMap
                    title="Input map"
                    side="input"
                    channels={audio.inputChannels}
                    onChange={onChange}
                />
                <ChannelMap
                    title="Output map"
                    side="output"
                    channels={audio.outputChannels}
                    onChange={onChange}
                />
            </div>

            <div className="audio-settings__readout">
                <dl className="audio-settings__latency" aria-label="Latency readout">
                    <div>
                        <dt>Buffer</dt>
                        <dd data-testid="latency-buffer">{formatMs (audio.latency.bufferMs)} ms</dd>
                    </div>
                    <div>
                        <dt>Device</dt>
                        <dd data-testid="latency-device">{formatMs (audio.latency.deviceMs)} ms</dd>
                    </div>
                    <div>
                        <dt>Chains</dt>
                        <dd data-testid="latency-chain">{formatMs (audio.latency.chainMs)} ms</dd>
                    </div>
                    <div className="audio-settings__latency-total">
                        <dt>Total</dt>
                        <dd data-testid="latency-total">{formatMs (audio.latency.totalMs)} ms</dd>
                    </div>
                </dl>

                <div className="audio-settings__xruns">
                    <span>Xruns</span>
                    <strong data-testid="xrun-count">{audio.xruns}</strong>
                    <span className="audio-settings__muted">
                        device {audio.deviceXruns} · callback {audio.callbackOverruns}
                    </span>
                </div>

                <div className="audio-settings__measure">
                    <button
                        type="button"
                        data-testid="measure-button"
                        disabled={busy || onMeasure === undefined}
                        onClick={() => { onMeasure?.(); }}
                    >
                        {busy ? "Working…" : "Measure"}
                    </button>
                    <Measurement measurement={measurement} error={measurementError} />
                </div>
            </div>

            <footer className="audio-settings__footer">
                <span>
                    {audio.open
                        ? `${formatSampleRate (audio.sampleRate)} · ${audio.bufferSize} frames · `
                          + `${String (activeChannels (audio.outputChannels).length)} out / `
                          + `${String (activeChannels (audio.inputChannels).length)} in`
                        : "Select a device to start audio"}
                </span>
                <span>{audio.callbacks.toLocaleString ("en-US")} callbacks</span>
            </footer>
        </section>
    );
}
