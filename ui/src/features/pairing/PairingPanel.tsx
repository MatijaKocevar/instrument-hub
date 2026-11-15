import type { ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import { PairingCodeSection } from "./PairingCodeSection";
import { PairingHeader } from "./PairingHeader";
import { PairingLanToggle } from "./PairingLanToggle";
import { PairingQrSection } from "./PairingQrSection";
import { PairingSkeleton } from "./PairingSkeleton";
import { PairingTokenSection } from "./PairingTokenSection";
import "./pairing.css";

export interface PairingPanelProps {
  pairing: ControlPairingState | null;
  token: string | null;
  busy?: boolean;
  loading?: boolean;
  onToggleLan?: (enabled: boolean) => void;
  onForget?: () => void;
  onPair?: (code: string) => void;
}

export function PairingPanel({
  pairing,
  token,
  busy = false,
  loading = false,
  onToggleLan,
  onForget,
  onPair,
}: PairingPanelProps): ReactElement {
  const lanEnabled = pairing?.lanEnabled === true;
  const code = pairing?.code ?? "";
  const pairedClients = pairing?.pairedClients ?? 0;
  const canForget = lanEnabled && pairedClients > 0 && !busy;
  const canToggle = !busy && !loading && onToggleLan !== undefined;

  if (loading && pairing === null) return <PairingSkeleton />;

  return (
    <section
      className="pairing"
      aria-label="Tablet pairing"
      aria-busy={busy}
      data-testid="pairing"
      data-lan={lanEnabled ? "true" : "false"}
      data-paired-clients={pairedClients}
    >
      <PairingHeader lanEnabled={lanEnabled} />

      <div className="pairing__actions">
        <PairingLanToggle
          lanEnabled={lanEnabled}
          disabled={!canToggle}
          onToggle={onToggleLan}
        />

        <PairingTokenSection
          token={token}
          code={code}
          lanEnabled={lanEnabled}
          busy={busy}
          onPair={onPair}
        />
      </div>

      {lanEnabled && pairing !== null ? (
        <div className="pairing__body">
          <PairingQrSection pairing={pairing} />
          <PairingCodeSection
            pairing={pairing}
            canForget={canForget}
            onForget={onForget}
          />
        </div>
      ) : null}
    </section>
  );
}
