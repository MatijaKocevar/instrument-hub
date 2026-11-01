import type { ReactElement } from "react";

interface PairingLanToggleProps {
  readonly lanEnabled: boolean;
  readonly disabled: boolean;
  readonly onToggle?: ((enabled: boolean) => void) | undefined;
}

export function PairingLanToggle({
  lanEnabled,
  disabled,
  onToggle,
}: PairingLanToggleProps): ReactElement {
  return (
    <button
      className={`pairing__toggle${lanEnabled ? " pairing__toggle--on" : ""}`}
      type="button"
      data-testid="lan-toggle"
      aria-pressed={lanEnabled}
      disabled={disabled}
      onClick={() => {
        onToggle?.(!lanEnabled);
      }}
    >
      {lanEnabled ? "Turn off LAN mode" : "Turn on LAN mode"}
    </button>
  );
}
