import { useState, type FormEvent, type ReactElement } from "react";

import type { ControlPairingState } from "../../api/client";
import { Spinner } from "../loading";
import { isPairingCode, normalizePairingCode } from "./model";
import "./pairing.css";

export interface PairingGateProps {
  pairing: ControlPairingState | null;
  busy?: boolean;
  error?: string | null;
  initialCode?: string | null;
  onPair: (code: string) => void;
}

export function PairingGate({
  pairing,
  busy = false,
  error = null,
  initialCode = null,
  onPair,
}: PairingGateProps): ReactElement {
  const [code, setCode] = useState(initialCode ?? "");
  const [submitted, setSubmitted] = useState(false);
  const valid = isPairingCode(code);
  const lanOff = pairing !== null && !pairing.lanEnabled;

  const submit = (event: FormEvent<HTMLFormElement>): void => {
    event.preventDefault();

    if (!valid || busy) return;

    setSubmitted(true);
    onPair(code);
  };

  return (
    <main
      className="pairing-gate"
      aria-label="Pair this tablet"
      data-testid="pairing-gate"
    >
      <header className="pairing-gate__header">
        <p className="pairing-gate__eyebrow">Instrument Hub</p>
        <h1 className="pairing-gate__title">Pair this tablet</h1>
      </header>

      <p className="pairing-gate__lede">
        {lanOff
          ? "LAN mode is off on the desktop. Turn it on there, then scan the QR code again."
          : "Enter the six-digit code shown on the desktop next to the QR code."}
      </p>

      {initialCode !== null && !submitted && !lanOff ? (
        <p className="pairing-gate__auto" data-testid="pairing-auto-code">
          Scanned code {initialCode.slice(0, 3)} {initialCode.slice(3)}.
        </p>
      ) : null}

      <form className="pairing-gate__form" onSubmit={submit}>
        <label className="pairing-gate__label" htmlFor="pairing-code">
          Pairing code
        </label>
        <input
          id="pairing-code"
          className="pairing-gate__input"
          data-testid="pairing-code-input"
          type="text"
          inputMode="numeric"
          autoComplete="one-time-code"
          pattern="[0-9]*"
          maxLength={6}
          value={code}
          disabled={busy || lanOff}
          onChange={(event) => {
            setCode(normalizePairingCode(event.target.value));
          }}
        />

        <button
          className="pairing-gate__submit"
          data-testid="pairing-submit"
          type="submit"
          disabled={!valid || busy || lanOff}
        >
          {busy ? <Spinner label="Pairing this browser" /> : null}
          {busy ? "Pairing…" : "Pair"}
        </button>
      </form>

      {error !== null ? (
        <p
          className="pairing-gate__error"
          role="alert"
          data-testid="pairing-gate-error"
        >
          {error}
        </p>
      ) : null}

      <p className="pairing-gate__hint" data-testid="pairing-gate-hint">
        The token stays in this browser; the desktop can forget it at any time.
      </p>
    </main>
  );
}
