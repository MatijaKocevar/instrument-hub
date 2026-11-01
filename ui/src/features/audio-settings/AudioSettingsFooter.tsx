import type { ReactElement } from "react";

import type { ControlAudioState } from "../../api/client";
import { activeChannels, formatSampleRate } from "./model";

interface AudioSettingsFooterProps
{
    readonly audio: ControlAudioState;
}

/** Live format summary and callback count, or the prompt to pick a device. */
export function AudioSettingsFooter ({ audio }: AudioSettingsFooterProps): ReactElement
{
    return (
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
    );
}
