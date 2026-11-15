import type { ReactElement } from "react";

import type { ControlAudioState } from "../../api/client";
import { activeChannels } from "./model";

interface AudioSettingsFooterProps {
  readonly audio: ControlAudioState;
}

export function AudioSettingsFooter({
  audio,
}: AudioSettingsFooterProps): ReactElement {
  return (
    <footer className="audio-settings__footer">
      <span>
        {audio.open
          ? `${String(activeChannels(audio.outputChannels).length)} out · ` +
            `${String(activeChannels(audio.inputChannels).length)} in`
          : "Select a device to start audio"}
      </span>
      <span>{audio.callbacks.toLocaleString("en-US")} callbacks</span>
    </footer>
  );
}
