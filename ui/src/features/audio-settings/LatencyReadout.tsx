import type { ReactElement } from "react";

import type { ControlAudioState } from "../../api/client";
import { formatMs } from "./model";

interface LatencyReadoutProps
{
    readonly latency: ControlAudioState["latency"];
}

export function LatencyReadout ({ latency }: LatencyReadoutProps): ReactElement
{
    return (
        <dl className="audio-settings__latency" aria-label="Latency readout">
            <div>
                <dt>Buffer</dt>
                <dd data-testid="latency-buffer">{formatMs (latency.bufferMs)} ms</dd>
            </div>
            <div>
                <dt>Device</dt>
                <dd data-testid="latency-device">{formatMs (latency.deviceMs)} ms</dd>
            </div>
            <div>
                <dt>Chains</dt>
                <dd data-testid="latency-chain">{formatMs (latency.chainMs)} ms</dd>
            </div>
            <div className="audio-settings__latency-total">
                <dt>Total</dt>
                <dd data-testid="latency-total">{formatMs (latency.totalMs)} ms</dd>
            </div>
        </dl>
    );
}
