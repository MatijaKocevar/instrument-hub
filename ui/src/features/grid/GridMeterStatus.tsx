import type { ReactElement } from "react";

import { meterPercent } from "./format";
import type { MeterSnapshot } from "./meterFrame";

export function GridMeterStatus ({ meters }: { readonly meters: MeterSnapshot }): ReactElement
{
    return (
        <div
            className={`grid__meters-status${meters.connected ? " grid__meters-status--live" : ""}`}
            data-testid="meters-status"
            data-connected={meters.connected ? "true" : "false"}
        >
            <span className="grid__meters-label">
                {meters.connected ? "Meters live" : "Meters idle"}
            </span>
            <span
                className="grid__master-meter"
                data-testid="master-meter"
                data-peak={meters.masterPeak.toFixed (3)}
                aria-hidden="true"
            >
                <span
                    className="grid__master-meter-fill"
                    style={{ width: `${String (meterPercent (meters.masterPeak))}%` }}
                />
            </span>
        </div>
    );
}
