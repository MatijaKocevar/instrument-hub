import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import type { ConnectionPhase } from "../../state/store";

const PHASE_LABEL: Record<ConnectionPhase, string> = {
  idle: "Idle",
  connecting: "Connecting",
  ready: "Connected",
  offline: "Offline",
};

interface AccountCardProps {
  readonly connection: ConnectionPhase;
  readonly baseUrl: string;
  readonly token: string | null;
  readonly pairedClients: number;
  readonly canRevoke: boolean;
  readonly loading?: boolean;
  readonly onForget?: (() => void) | undefined;
}

export function AccountCard({
  connection,
  baseUrl,
  token,
  pairedClients,
  canRevoke,
  loading = false,
  onForget,
}: AccountCardProps): ReactElement {
  return (
    <section
      className="settings__card"
      aria-labelledby="settings-account-title"
      data-testid="settings-account"
    >
      <header className="settings__head">
        <p className="settings__eyebrow">Account</p>
        <h3 className="settings__title" id="settings-account-title">
          This browser
        </h3>
      </header>

      <dl className="settings__meta">
        <div>
          <dt>Connection</dt>
          <dd data-testid="settings-connection" data-phase={connection}>
            {PHASE_LABEL[connection]}
          </dd>
        </div>
        <div>
          <dt>Control API</dt>
          <dd>
            <code data-testid="settings-endpoint">{baseUrl}</code>
          </dd>
        </div>
        <div>
          <dt>Pairing token</dt>
          <dd
            data-testid="settings-token-state"
            data-paired={token === null ? "false" : "true"}
          >
            {token === null ? "Not paired" : "Stored in this browser"}
          </dd>
        </div>
        <div>
          <dt>Paired tablets</dt>
          <dd data-testid="settings-paired-clients">
            {loading ? (
              <Skeleton width="2rem" height="0.85rem" />
            ) : (
              pairedClients
            )}
          </dd>
        </div>
      </dl>

      <button
        className="settings__button"
        type="button"
        data-testid="settings-revoke-token"
        disabled={!canRevoke}
        onClick={() => {
          onForget?.();
        }}
      >
        Revoke pairing tokens
      </button>
      <p className="settings__hint">
        Revoking signs out every paired tablet. This desktop re-pairs itself
        automatically while LAN mode stays on.
      </p>
    </section>
  );
}
