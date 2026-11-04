import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import {
  MidiPanel,
  canRunPadTest,
  isLaunchpadDaInput,
  midiGridConfigured,
  midiGridLabel,
  midiHandshakeLabel,
  midiInputForRole,
  midiInputState,
  midiInputSummary,
  midiLaunchpadSummary,
  midiPortLabel,
  midiRoleLabel,
  padTestLabel,
} from "../../features/midi";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { MIDI } from "../fixtures";
import { check, countOf, renderApp, writeArtifact } from "../harness";

export function midiModelTest(): void {
  check(
    midiRoleLabel("launchpad") === "Launchpad",
    "the Launchpad role has a label",
  );
  check(
    midiRoleLabel("keyboard") === "Keyboard",
    "any keyboard input has a keyboard label",
  );
  check(
    midiRoleLabel("grid") === "Pad grid",
    "a generic pad controller has a grid label",
  );

  check(
    midiInputSummary(MIDI.inputs) === "5 inputs · 1 enabled",
    "the summary counts inputs and enabled switches",
  );
  check(
    midiInputSummary([MIDI.inputs[0]!]) === "1 input · 1 enabled",
    "the singular summary is used for one input",
  );
  check(
    midiInputSummary([]) === "No MIDI inputs",
    "an empty device list is explained",
  );
  check(
    midiLaunchpadSummary(MIDI.inputs) === "Launchpad ready",
    "the Launchpad summary reports a completed handshake",
  );

  check(
    midiInputState(MIDI.inputs[0]!) === "Open",
    "an enabled open input reads Open",
  );
  check(
    midiInputState(MIDI.inputs[2]!) === "Off",
    "a disabled input reads Off",
  );

  check(
    midiPortLabel("MI") === "MI" && midiPortLabel("  ") === "—",
    "the port label falls back to a dash",
  );
  check(
    midiHandshakeLabel(MIDI.inputs[0]!) === "Programmer mode",
    "the Launchpad shows its programmer-mode handshake",
  );
  check(
    midiHandshakeLabel(MIDI.inputs[1]!) === "No handshake",
    "an offline Launchpad shows the missing handshake",
  );
  check(
    midiHandshakeLabel(MIDI.inputs[2]!) === "—",
    "a keyboard has no handshake to report",
  );
  check(
    midiHandshakeLabel(MIDI.inputs[4]!) === "No LED output",
    "a generic grid says that its protocol has no LEDs",
  );

  check(
    midiGridLabel(MIDI.inputs[4]!) === "Notes 36–99",
    "a note-base grid names its note range",
  );
  check(
    midiGridConfigured(MIDI.inputs[4]!),
    "a note-base grid counts as configured",
  );

  check(
    isLaunchpadDaInput(MIDI.inputs[1]!) && !isLaunchpadDaInput(MIDI.inputs[0]!),
    "the inert DA port is distinguished from MI",
  );

  check(
    midiInputForRole(MIDI.inputs, "keyboard")?.id === "ALSA:5:0",
    "the first keyboard row is found by role",
  );
  check(
    midiInputForRole(MIDI.inputs, "launchpad")?.id === "ALSA:4:1",
    "the Launchpad row prefers the MI port",
  );
  check(
    midiInputForRole(MIDI.inputs, "grid")?.id === "ALSA:6:0",
    "the grid row is found by role",
  );

  check(
    padTestLabel(null, null) === "Not run yet",
    "a fresh page has not run the test",
  );
  check(
    padTestLabel({ active: true, lit: 64, durationMs: 900 }, null) ===
      "64 pads lit",
    "a running test reports the lit pad count",
  );
  check(
    padTestLabel({ active: false, lit: 0, durationMs: 900 }, "no Launchpad") ===
      "no Launchpad",
    "a rejected test shows the engine reason",
  );

  check(
    canRunPadTest(MIDI),
    "the pad test is available with a connected Launchpad",
  );
  check(
    !canRunPadTest({
      ...MIDI,
      inputs: MIDI.inputs.map((input) => ({ ...input, open: false })),
    }),
    "the pad test is unavailable without an open Launchpad",
  );
}

export async function midiPageTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    check(await store.refresh(), "the MIDI fixture loads GET /midi");
    check(
      store.getSnapshot().midi?.inputs.length === 5,
      "the store holds all five MIDI inputs",
    );

    const initial = renderApp(store, "/audio");

    writeArtifact("ui-audio-midi-snapshot.html", initial);

    check(
      initial.includes('data-testid="midi-panel"'),
      "the /audio page renders the MIDI panel",
    );
    check(
      countOf(initial, 'data-testid="midi-input"') === 5,
      "the panel lists every MIDI input",
    );
    check(
      initial.includes('data-testid="midi-input" data-id="ALSA:4:1"'),
      "the Launchpad MI input is listed",
    );
    check(
      initial.includes(
        'data-id="ALSA:4:1" data-name="Launchpad Mini MK3 LPMiniMK3 MI" data-role="launchpad"',
      ),
      "the Launchpad input carries its role",
    );
    check(
      initial.includes(
        'data-testid="midi-input-handshake" data-handshake="true"',
      ),
      "the Launchpad row shows the programmer-mode handshake",
    );
    check(
      initial.includes("Launchpad ready"),
      "the summary names the ready Launchpad",
    );
    check(
      initial.includes(
        'data-id="ALSA:5:0" data-name="Keystation Mini 32 MK3 USB Audio" data-role="keyboard"',
      ),
      "a generic keyboard is listed as a keyboard",
    );
    check(
      initial.includes('data-testid="midi-input-role" data-id="ALSA:5:0"'),
      "a generic input offers the keyboard/grid role switch",
    );
    check(
      initial.includes('data-testid="midi-input" data-id="ALSA:6:0"'),
      "the generic pad controller is listed",
    );
    check(
      initial.includes('data-testid="midi-grid-controls" data-id="ALSA:6:0"'),
      "the grid row shows its note-base and learn controls",
    );
    check(initial.includes("Notes 36–99"), "the grid row names its note range");
    check(
      initial.includes('data-testid="midi-grid-learn"'),
      "the grid row offers a learn action",
    );
    check(
      initial.includes("No LED output"),
      "the generic grid reports that its LEDs are unsupported",
    );
    check(
      initial.includes('data-id="ALSA:4:0"') &&
        initial.includes("DA port carries no pads or LEDs"),
      "the unusable DA port explains itself",
    );
    check(
      /data-testid="midi-input-toggle" data-id="ALSA:4:0"[^>]*disabled/.test(
        initial,
      ),
      "the DA checkbox cannot be switched on",
    );
    check(
      !initial.includes('data-testid="launchpad-status"') &&
        !initial.includes('data-testid="keystation-status"'),
      "the large Launchpad and Keystation cards are gone",
    );
    check(
      initial.includes('data-testid="pad-test"'),
      "the page offers the pad test",
    );
    check(
      initial.includes(
        'data-testid="pad-test-result" data-active="false" data-lit="0"',
      ),
      "the pad test has not run yet",
    );
    check(
      /data-testid="pad-test"[^>]*disabled/.test(initial) === false,
      "the pad test is available while the Launchpad is connected",
    );

    check(
      await store.updateMidiInput({ id: "ALSA:5:0", enabled: true }),
      "enabling the generic keyboard succeeds",
    );
    check(
      server.midiRequests.length === 1 &&
        server.midiRequests[0]?.id === "ALSA:5:0" &&
        server.midiRequests[0]?.enabled === true,
      "the UI sent { id, enabled: true } to POST /midi/inputs",
    );
    check(
      store.getSnapshot().midi?.inputs.find((input) => input.id === "ALSA:5:0")
        ?.enabled === true,
      "the keyboard row is switched on",
    );

    const enabled = renderApp(store, "/audio");

    check(
      /data-testid="midi-input" data-id="ALSA:5:0"[^>]*data-enabled="true"/.test(
        enabled,
      ),
      "the keyboard row renders as enabled",
    );
    check(
      /data-testid="midi-input-toggle" data-id="ALSA:5:0"[^>]*checked/.test(
        enabled,
      ),
      "the keyboard checkbox is checked",
    );
    check(
      enabled.includes("2 enabled"),
      "the summary follows the second switch",
    );

    check(
      await store.updateMidiInput({
        id: "ALSA:2:0",
        role: "grid",
        enabled: true,
      }),
      "moving a generic input to the grid role succeeds",
    );
    check(
      server.midiRequests[1]?.role === "grid",
      "the UI sent the grid role to POST /midi/inputs",
    );
    check(
      store.getSnapshot().midi?.inputs.find((input) => input.id === "ALSA:2:0")
        ?.role === "grid",
      "the input becomes a grid in the store",
    );

    const gridRole = renderApp(store, "/audio");

    check(
      gridRole.includes('data-testid="midi-grid-controls" data-id="ALSA:2:0"'),
      "the new grid row grows its controls",
    );

    check(
      await store.updateMidiInput({ id: "ALSA:2:0", noteBase: 48 }),
      "setting a grid note base succeeds",
    );
    check(
      store.getSnapshot().midi?.inputs.find((input) => input.id === "ALSA:2:0")
        ?.noteBase === 48,
      "the store adopts the note base",
    );

    check(
      await store.updateMidiInput({ id: "ALSA:2:0", learn: true }),
      "starting a 64-pad learn succeeds",
    );
    check(
      server.midiRequests[3]?.learn === true,
      "the UI asked the engine to learn",
    );
    check(
      store.getSnapshot().midi?.inputs.find((input) => input.id === "ALSA:2:0")
        ?.learnActive === true,
      "the store reports the running learn",
    );

    const learning = renderApp(store, "/audio");

    check(
      learning.includes('data-testid="midi-grid-hint"'),
      "the learning row tells the user which order to press",
    );
    check(
      learning.includes("Learning 0 of 64 pads"),
      "the learning row reports its progress",
    );

    check(
      await store.updateMidiInput({ id: "ALSA:2:0", learn: false }),
      "cancelling the learn succeeds",
    );
    check(
      store.getSnapshot().midi?.inputs.find((input) => input.id === "ALSA:2:0")
        ?.learnActive === false,
      "the cancelled learn stops",
    );

    check(await store.runPadTest(), "POST /midi/pad-test starts the grid test");
    check(server.padTestRequests === 1, "the pad test reached the engine");
    check(
      store.getSnapshot().midi?.padTest.active === true,
      "the store adopts the running test",
    );
    check(
      store.getSnapshot().midi?.padTest.lit === 64,
      "the test reports all 64 pads lit",
    );

    const testing = renderApp(store, "/audio");

    check(
      testing.includes('data-testid="midi-panel"') &&
        testing.includes('data-pad-test-active="true"'),
      "the panel reports the running test",
    );
    check(
      testing.includes(
        'data-testid="pad-test-result" data-active="true" data-lit="64"',
      ),
      "the result names the 64 lit pads",
    );
    check(testing.includes("64 pads lit"), "the result is readable");

    const failing = new AppStore(
      createControlApi({ baseUrl: "http://127.0.0.1:1" }),
      { pollIntervalMs: 60_000 },
    );

    check(!(await failing.loadMidi()), "a failed MIDI read is reported");
    check(
      failing.getSnapshot().midiError !== null,
      "the store keeps the MIDI failure message",
    );

    const failureHtml = renderApp(failing, "/audio");

    check(
      failureHtml.includes('data-testid="midi-panel" data-loading="true"'),
      "the MIDI panel shows its loading/failure state",
    );
    check(
      failureHtml.includes('data-testid="midi-loading"'),
      "the failure is shown in place of the device list",
    );

    const loadingHtml = renderToStaticMarkup(<MidiPanel midi={null} loading />);

    check(
      loadingHtml.includes('data-testid="midi-loading"'),
      "a first load shows the reading state",
    );
    check(
      loadingHtml.includes('aria-busy="true"'),
      "the loading panel is marked busy",
    );
  } finally {
    await server.close();
  }
}

export function assertSnapshotMidi(
  audioHtml: string,
  fromFixture: boolean,
): void {
  if (!fromFixture) return;

  check(
    countOf(audioHtml, 'data-testid="midi-input"') === 5,
    "the MIDI panel lists every MIDI input",
  );
  check(
    audioHtml.includes('data-testid="midi-input" data-id="ALSA:4:1"'),
    "the Launchpad MI input is listed",
  );
  check(
    audioHtml.includes(
      'data-testid="midi-input-handshake" data-handshake="true"',
    ),
    "the Launchpad programming handshake is shown in its row",
  );
  check(
    audioHtml.includes('data-testid="midi-input" data-id="ALSA:5:0"'),
    "the generic keyboard input is listed",
  );
  check(
    audioHtml.includes('data-testid="midi-grid-controls" data-id="ALSA:6:0"'),
    "the generic grid row keeps its controls",
  );
  check(
    audioHtml.includes(
      'data-testid="pad-test-result" data-active="false" data-lit="0"',
    ),
    "the pad test has not run yet",
  );
}
