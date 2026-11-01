import type { ReactElement } from "react";

export function SettingsHeader(): ReactElement {
  return (
    <header className="page__header">
      <p className="page__eyebrow">Settings &amp; Account</p>
      <h2 className="page__title">Settings &amp; Account</h2>
      <p className="page__lede">
        Pair a tablet on your local network, manage this browser's access, and
        check which engine build is running.
      </p>
    </header>
  );
}
