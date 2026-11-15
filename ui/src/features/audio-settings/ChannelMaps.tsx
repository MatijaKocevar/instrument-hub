import type { ReactElement } from "react";

import type { ControlAudioChannel, ControlAudioPatch } from "../../api/client";
import { activeChannels, channelSummary, toggleChannel } from "./model";

function ChannelMap({
  title,
  side,
  channels,
  onChange,
}: {
  title: string;
  side: "input" | "output";
  channels: ControlAudioChannel[];
  onChange?: ((patch: ControlAudioPatch) => void) | undefined;
}): ReactElement {
  return (
    <fieldset
      className="audio-settings__channels"
      data-testid={`${side}-channels`}
    >
      <legend>
        {title}
        <span className="audio-settings__channel-summary">
          {channelSummary(channels)}
        </span>
      </legend>

      <div className="audio-settings__channel-list">
        {channels.length === 0 ? (
          <span className="audio-settings__muted">No channels</span>
        ) : (
          channels.map((channel) => (
            <label
              className="audio-settings__channel"
              key={`${side}-${String(channel.index)}`}
            >
              <input
                type="checkbox"
                data-testid={`${side}-channel`}
                data-index={channel.index}
                checked={channel.active}
                onChange={() => {
                  if (onChange === undefined) return;

                  onChange(
                    side === "input"
                      ? {
                          inputChannels: toggleChannel(channels, channel.index),
                        }
                      : {
                          outputChannels: toggleChannel(
                            channels,
                            channel.index,
                          ),
                        },
                  );
                }}
              />
              <span>{channel.name}</span>
              <code>{channel.index}</code>
            </label>
          ))
        )}
      </div>
    </fieldset>
  );
}

interface ChannelMapsProps {
  readonly inputChannels: ControlAudioChannel[];
  readonly outputChannels: ControlAudioChannel[];
  readonly onChange?: ((patch: ControlAudioPatch) => void) | undefined;
}

export function ChannelMaps({
  inputChannels,
  outputChannels,
  onChange,
}: ChannelMapsProps): ReactElement {
  return (
    <details className="audio-settings__section" data-testid="channel-maps">
      <summary className="audio-settings__section-summary">
        Channels
        <span className="audio-settings__section-hint">
          {activeChannels(outputChannels).length} out ·{" "}
          {activeChannels(inputChannels).length} in
        </span>
      </summary>

      <p className="audio-settings__section-note">
        Enable only the channels your interface actually uses. Fewer active
        channels keep the audio callback cheap.
      </p>

      <div className="audio-settings__maps">
        <ChannelMap
          title="Input map"
          side="input"
          channels={inputChannels}
          onChange={onChange}
        />
        <ChannelMap
          title="Output map"
          side="output"
          channels={outputChannels}
          onChange={onChange}
        />
      </div>
    </details>
  );
}
