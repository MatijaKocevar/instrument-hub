import type { ReactElement } from "react";

interface PairingHeaderProps
{
    readonly lanEnabled: boolean;
}

export function PairingHeader ({ lanEnabled }: PairingHeaderProps): ReactElement
{
    return (
        <>
            <header className="pairing__header">
                <div>
                    <p className="pairing__eyebrow">Remote</p>
                    <h2 className="pairing__title">Tablet pairing</h2>
                </div>
                <span
                    className={`status-pill pairing__status${lanEnabled ? " pairing__status--on" : ""}`}
                    data-testid="pairing-lan-status"
                >
                    {lanEnabled ? "LAN mode on" : "LAN mode off"}
                </span>
            </header>

            <p className="pairing__lede">
                {lanEnabled
                    ? "Scan this code with the tablet camera. The code is single-use and expires; a fresh one appears here automatically."
                    : "LAN mode is off, so only this computer can reach the engine. Turn it on to pair a tablet on the same network."}
            </p>
        </>
    );
}
