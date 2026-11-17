import { useSyncExternalStore, type ReactElement } from "react";

import { useWakeLock } from "../remote/useWakeLock";
import { midiPortLabel } from "./webMidi";
import type { MidiBridgeController } from "./useMidiBridgeController";
import type { BridgeLinkStatus } from "./useMidiBridgeLink";
import "./midiBridge.css";

const LINK_LABEL: Record<BridgeLinkStatus, string> = {
  idle: "Off",
  connecting: "Linking…",
  linked: "Linked",
  reconnecting: "Reconnecting…",
  closed: "Closed",
  error: "Link error",
};

export interface MidiBridgeStageProps {
  readonly controller: MidiBridgeController;
  readonly onBack: () => void;
}

export function MidiBridgeStage({
  controller,
  onBack,
}: MidiBridgeStageProps): ReactElement {
  const wakeLock = useWakeLock();
  const telemetry = useSyncExternalStore(
    controller.telemetry.subscribe,
    controller.telemetry.getSnapshot,
    controller.telemetry.getSnapshot,
  );
  const canToggle =
    controller.permission === "ready" && controller.selectedId !== null;
  const linkReady = controller.linkStatus === "linked";
  const origin = typeof window === "undefined" ? "" : window.location.origin;

  return (
    <main
      className="midi-bridge"
      aria-label="MIDI over WiFi"
      data-testid="midi-bridge"
      data-support={controller.support}
      data-link={controller.linkStatus}
      data-bridge={controller.bridgeOn ? "on" : "off"}
    >
      <header className="midi-bridge__header">
        <button
          className="midi-bridge__back"
          type="button"
          data-testid="midi-bridge-back"
          onClick={onBack}
        >
          ← Stage
        </button>

        <div className="midi-bridge__heading">
          <p className="midi-bridge__eyebrow">Instrument Hub</p>
          <h1 className="midi-bridge__title">MIDI over WiFi</h1>
        </div>

        <span
          className={`midi-bridge__link midi-bridge__link--${controller.linkStatus}`}
          data-testid="midi-bridge-link"
          data-status={controller.linkStatus}
        >
          {LINK_LABEL[controller.linkStatus]}
        </span>
      </header>

      {controller.support === "insecure" ? (
        <section className="midi-bridge__card midi-bridge__card--warning">
          <h2>Open the secure page</h2>
          <p>
            Web MIDI only runs on HTTPS. This page is loaded from{" "}
            <code>{origin}</code>, so the browser hides MIDI.
          </p>
          <p>
            On the desktop, open the pairing panel, keep LAN mode on, and scan
            the QR code or open the HTTPS address shown there.
          </p>
        </section>
      ) : null}

      {controller.support === "unsupported" ? (
        <section className="midi-bridge__card midi-bridge__card--warning">
          <h2>This browser cannot read MIDI</h2>
          <p>
            Use Chrome on Android. Safari on iPhone has no Web MIDI, and Chrome
            on iOS uses the same engine.
          </p>
        </section>
      ) : null}

      {controller.support === "available" ? (
        <section className="midi-bridge__card">
          <h2>Keyboard</h2>

          {controller.permission === "idle" ? (
            <>
              <p>
                Connect the keyboard to the phone with a USB OTG adapter, then
                allow MIDI access.
              </p>
              <button
                className="midi-bridge__primary"
                type="button"
                data-testid="midi-allow"
                onClick={() => {
                  void controller.requestAccess();
                }}
              >
                Allow MIDI devices
              </button>
            </>
          ) : null}

          {controller.permission === "requesting" ? (
            <p data-testid="midi-permission">Waiting for MIDI permission…</p>
          ) : null}

          {controller.permission === "denied" ? (
            <>
              <p className="midi-bridge__error" data-testid="midi-error">
                {controller.error ?? "MIDI access was denied"}
              </p>
              <button
                className="midi-bridge__primary"
                type="button"
                onClick={() => {
                  void controller.requestAccess();
                }}
              >
                Try again
              </button>
            </>
          ) : null}

          {controller.permission === "ready" &&
          controller.ports.length === 0 ? (
            <p data-testid="midi-empty">
              No MIDI input yet. Plug the keyboard into the phone and power it
              on; it should appear here without a reload.
            </p>
          ) : null}

          {controller.permission === "ready" && controller.ports.length > 0 ? (
            <label className="midi-bridge__field" htmlFor="midi-input">
              <span>Input</span>
              <select
                id="midi-input"
                data-testid="midi-input-select"
                value={controller.selectedId ?? ""}
                onChange={(event) => {
                  controller.selectPort(
                    event.currentTarget.value.length > 0
                      ? event.currentTarget.value
                      : null,
                  );
                }}
              >
                {controller.ports.map((port) => (
                  <option key={port.id} value={port.id}>
                    {midiPortLabel(port)}
                  </option>
                ))}
              </select>
            </label>
          ) : null}

          {controller.permission === "ready" ? (
            <>
              <button
                className={`midi-bridge__primary${controller.bridgeOn ? " midi-bridge__primary--stop" : ""}`}
                type="button"
                data-testid="midi-bridge-toggle"
                disabled={!canToggle && !controller.bridgeOn}
                onClick={() => {
                  controller.setBridgeOn(!controller.bridgeOn);
                }}
              >
                {controller.bridgeOn
                  ? "Turn off WiFi MIDI"
                  : "Turn on WiFi MIDI"}
              </button>

              <p className="midi-bridge__readout" data-testid="midi-readout">
                {!controller.bridgeOn
                  ? "Turn it on once, then switch tracks freely."
                  : !linkReady
                    ? "WiFi MIDI is on; waiting for the desktop link before sending."
                    : `${String(telemetry.count)} messages sent · last ${telemetry.lastEvent}`}
              </p>

              {controller.bridgeOn ? (
                <p className="midi-bridge__sticky">
                  WiFi MIDI stays on while you switch to the stage and change
                  tracks.
                </p>
              ) : null}
            </>
          ) : null}
        </section>
      ) : null}

      <section className="midi-bridge__card midi-bridge__card--status">
        <h2>Link</h2>
        <dl className="midi-bridge__facts">
          <div>
            <dt>Desktop</dt>
            <dd data-testid="midi-link-status">
              {LINK_LABEL[controller.linkStatus]}
            </dd>
          </div>
          <div>
            <dt>Client</dt>
            <dd>{controller.clientId.slice(0, 8)}</dd>
          </div>
          <div>
            <dt>Screen</dt>
            <dd>{wakeLock === "active" ? "Kept on" : "May sleep when idle"}</dd>
          </div>
        </dl>

        {controller.linkError !== null ? (
          <p className="midi-bridge__error">{controller.linkError}</p>
        ) : null}

        <button
          className="midi-bridge__secondary"
          type="button"
          disabled={!controller.bridgeOn || !linkReady}
          onClick={controller.panic}
        >
          Panic — all notes off
        </button>

        <button
          className="midi-bridge__secondary"
          type="button"
          disabled={!controller.bridgeOn}
          onClick={controller.reconnect}
        >
          Reconnect
        </button>
      </section>

      <p className="midi-bridge__hint">
        Keep this tab open and visible. Chrome throttles background tabs, and
        the engine releases held notes when the link drops.
      </p>
    </main>
  );
}
