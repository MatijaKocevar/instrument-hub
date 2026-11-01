import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import { pairingPayload } from "./model";
import { QrCodeImage } from "./QrCodeImage";

interface PairingQrSectionProps {
  readonly pairing: ControlPairingState;
}

export function PairingQrSection({
  pairing,
}: PairingQrSectionProps): ReactElement {
  return (
    <div className="pairing__qr-column">
      <QrCodeImage value={pairingPayload(pairing)} size={228} />
      <p className="pairing__endpoint" data-testid="pairing-endpoint">
        {pairing.address}:{pairing.port}
      </p>
    </div>
  );
}
