import type { ReactElement } from "react";

import type { ControlAudioState } from "../../api/client";
import { deviceLabel } from "./model";

interface AudioSettingsHeaderProps
{
    readonly audio: ControlAudioState | null;
}

export function AudioSettingsHeader ({ audio }: AudioSettingsHeaderProps): ReactElement
{
    if (audio === null)
    {
        return (
            <header className="audio-settings__header">
                <h2 className="audio-settings__title">Audio settings</h2>
                <span className="audio-settings__status" data-testid="audio-status">
                    Loading
                </span>
            </header>
        );
    }

    return (
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
    );
}
