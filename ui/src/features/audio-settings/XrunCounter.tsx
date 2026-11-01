import type { ReactElement } from "react";

interface XrunCounterProps
{
    readonly xruns: number;
    readonly deviceXruns: number;
    readonly callbackOverruns: number;
}

/** The total xrun count with its device and callback breakdown. */
export function XrunCounter ({ xruns, deviceXruns, callbackOverruns }: XrunCounterProps): ReactElement
{
    return (
        <div className="audio-settings__xruns">
            <span>Xruns</span>
            <strong data-testid="xrun-count">{xruns}</strong>
            <span className="audio-settings__muted">
                device {deviceXruns} · callback {callbackOverruns}
            </span>
        </div>
    );
}
