import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import { pairingPayload } from "./model";
import { QrCodeImage } from "./QrCodeImage";

interface PairingQrSectionProps
{
    /** Live pairing state; the payload and endpoint come from it. */
    readonly pairing: ControlPairingState;
}

/** The QR a tablet scans and the endpoint it points at. */
export function PairingQrSection ({ pairing }: PairingQrSectionProps): ReactElement
{
    return (
        <div className="pairing__qr-column">
            <QrCodeImage value={pairingPayload (pairing)} size={228} />
            <p className="pairing__endpoint" data-testid="pairing-endpoint">
                {pairing.address}:{pairing.port}
            </p>
        </div>
    );
}
