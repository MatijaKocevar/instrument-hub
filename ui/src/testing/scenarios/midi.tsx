import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import {
    MidiPanel,
    canRunPadTest,
    isLaunchpadDaInput,
    midiInputForRole,
    midiInputState,
    midiInputSummary,
    midiPortLabel,
    midiPortState,
    midiRoleLabel,
    padTestLabel,
} from "../../features/midi";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { MIDI } from "../fixtures";
import { check, countOf, renderApp, writeArtifact } from "../harness";

export function midiModelTest (): void
{
    check (midiRoleLabel ("launchpad") === "Launchpad", "the Launchpad role has a label");
    check (midiRoleLabel ("keystation") === "Keyboard", "the Keystation is shown as a keyboard");
    check (midiRoleLabel ("input") === "MIDI input", "any other port is a MIDI input");

    check (midiInputSummary (MIDI.inputs) === "4 inputs · 1 enabled",
           "the summary counts inputs and enabled switches");
    check (midiInputSummary ([MIDI.inputs[0]!]) === "1 input · 1 enabled",
           "the singular summary is used for one input");
    check (midiInputSummary ([]) === "No MIDI inputs", "an empty device list is explained");

    check (midiInputState (MIDI.inputs[0]!) === "Open", "an enabled open input reads Open");
    check (midiInputState (MIDI.inputs[2]!) === "Off", "a disabled input reads Off");

    check (midiPortLabel ("MI") === "MI" && midiPortLabel ("  ") === "—",
           "the port label falls back to a dash");
    check (midiPortState (MIDI.launchpad, true) === "Connected",
           "a connected port says Connected");
    check (midiPortState (MIDI.keystation, true) === "Detected · not connected",
           "a detected but closed port says so");

    check (isLaunchpadDaInput (MIDI.inputs[1]!) && ! isLaunchpadDaInput (MIDI.inputs[0]!),
           "the inert DA port is distinguished from MI");

    check (midiInputForRole (MIDI.inputs, "keystation")?.id === "ALSA:5:0",
           "the Keystation row is found by role");
    check (midiInputForRole (MIDI.inputs, "launchpad")?.id === "ALSA:4:1",
           "the Launchpad row prefers the MI port");
    check (midiInputForRole (MIDI.inputs, "input")?.name === "Steinberg UR22mkII MIDI 1",
           "a plain input is found by role");

    check (padTestLabel (null, null) === "Not run yet", "a fresh page has not run the test");
    check (padTestLabel ({ active: true, lit: 64, durationMs: 900 }, null) === "64 pads lit",
           "a running test reports the lit pad count");
    check (padTestLabel ({ active: false, lit: 0, durationMs: 900 }, "no Launchpad") === "no Launchpad",
           "a rejected test shows the engine reason");

    check (canRunPadTest (MIDI), "the pad test is available with a connected Launchpad");
    check (! canRunPadTest ({ ...MIDI, launchpad: { ...MIDI.launchpad, connected: false } }),
           "the pad test is unavailable without a Launchpad");
}

export async function midiPageTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        check (await store.refresh (), "the MIDI fixture loads GET /midi");
        check (store.getSnapshot ().midi?.inputs.length === 4,
               "the store holds all four ALSA inputs");
        check (store.getSnapshot ().midi?.launchpad.port === "MI",
               "the Launchpad is reported on the MI port");

        const initial = renderApp (store, "/audio");

        writeArtifact ("ui-audio-midi-snapshot.html", initial);

        check (initial.includes ('data-testid="midi-panel"'), "the /audio page renders the MIDI panel");
        check (countOf (initial, 'data-testid="midi-input"') === 4,
               "the panel lists every ALSA input");
        check (initial.includes ('data-testid="midi-input" data-id="ALSA:4:1"'),
               "the Launchpad MI input is listed");
        check (initial.includes ('data-testid="midi-input" data-id="ALSA:4:1"')
                   && initial.includes ('data-role="launchpad"'),
               "the Launchpad input carries its role");
        check (initial.includes ('data-testid="midi-input" data-id="ALSA:5:0"'),
               "the Keystation input is listed");
        check (initial.includes ('data-id="ALSA:4:0"') && initial.includes ("MI only"),
               "the inert DA port is shown as MI-only");
        check (/data-testid="midi-input-toggle" data-id="ALSA:4:0"[^>]*disabled/.test (initial),
               "the DA checkbox cannot be switched on");
        check (initial.includes ('data-testid="launchpad-status" data-connected="true" data-port="MI"'),
               "the Launchpad status names the MI port");
        check (initial.includes ('data-testid="launchpad-port" data-port="MI">MI<'),
               "the Launchpad port pill reads MI");
        check (initial.includes ("LPMiniMK3 MI"),
               "the Launchpad device name is visible");
        check (initial.includes ('data-testid="launchpad-handshake" data-handshake="true"'),
               "the programmer-mode echo is shown");
        check (initial.includes ('data-testid="keystation-status" data-connected="false" data-port="USB"'),
               "the Keystation is detected but not connected");
        check (initial.includes ('data-testid="keystation-port" data-port="USB">USB<'),
               "the Keystation port pill reads USB");
        check (initial.includes ('data-testid="pad-test"'), "the page offers the pad test");
        check (initial.includes ('data-testid="pad-test-result" data-active="false" data-lit="0"'),
               "the pad test has not run yet");
        check (/data-testid="pad-test"[^>]*disabled/.test (initial) === false,
               "the pad test is available while the Launchpad is connected");

        check (await store.toggleMidiInput ("ALSA:5:0", true),
               "enabling the Keystation succeeds");
        check (server.midiRequests.length === 1
                   && server.midiRequests[0]?.id === "ALSA:5:0"
                   && server.midiRequests[0]?.enabled === true,
               "the UI sent { id, enabled: true } to POST /midi/inputs");
        check (store.getSnapshot ().midi?.keystation.connected === true,
               "the store adopts the opened Keystation");
        check (store.getSnapshot ().midi?.inputs.find (input => input.id === "ALSA:5:0")?.enabled === true,
               "the Keystation row is switched on");

        const enabled = renderApp (store, "/audio");

        check (/data-testid="midi-input" data-id="ALSA:5:0"[^>]*data-enabled="true"/.test (enabled),
               "the Keystation row renders as enabled");
        check (/data-testid="midi-input-toggle" data-id="ALSA:5:0"[^>]*checked/.test (enabled),
               "the Keystation checkbox is checked");
        check (enabled.includes ("2 enabled"), "the summary follows the second switch");

        check (await store.runPadTest (), "POST /midi/pad-test starts the grid test");
        check (server.padTestRequests === 1, "the pad test reached the engine");
        check (store.getSnapshot ().midi?.padTest.active === true,
               "the store adopts the running test");
        check (store.getSnapshot ().midi?.padTest.lit === 64,
               "the test reports all 64 pads lit");

        const testing = renderApp (store, "/audio");

        check (testing.includes ('data-testid="midi-panel"')
                   && testing.includes ('data-pad-test-active="true"'),
               "the panel reports the running test");
        check (testing.includes ('data-testid="pad-test-result" data-active="true" data-lit="64"'),
               "the result names the 64 lit pads");
        check (testing.includes ("64 pads lit"), "the result is readable");
        check (testing.includes ("Lighting…"), "the button shows the test is running");

        const failing = new AppStore (
            createControlApi ({ baseUrl: "http://127.0.0.1:1" }),
            { pollIntervalMs: 60_000 });

        check (! await failing.loadMidi (), "a failed MIDI read is reported");
        check (failing.getSnapshot ().midiError !== null,
               "the store keeps the MIDI failure message");

        const failureHtml = renderApp (failing, "/audio");

        check (failureHtml.includes ('data-testid="midi-panel" data-loading="true"'),
               "the MIDI panel shows its loading/failure state");
        check (failureHtml.includes ('data-testid="midi-loading"'),
               "the failure is shown in place of the device list");

        const loadingHtml = renderToStaticMarkup (
            <MidiPanel midi={null} loading />);

        check (loadingHtml.includes ('data-testid="midi-loading"'),
               "a first load shows the reading state");
        check (loadingHtml.includes ('aria-busy="true"'), "the loading panel is marked busy");
    }
    finally
    {
        await server.close ();
    }
}

export function assertSnapshotMidi (audioHtml: string, fromFixture: boolean): void
{
    if (! fromFixture)
        return;

    check (countOf (audioHtml, 'data-testid="midi-input"') === 4,
           "the MIDI panel lists every ALSA input");
    check (audioHtml.includes ('data-testid="midi-input" data-id="ALSA:4:1"'),
           "the Launchpad MI input is listed");
    check (audioHtml.includes ('data-testid="midi-input" data-id="ALSA:5:0"'),
           "the Keystation input is listed");
    check (audioHtml.includes ('data-testid="launchpad-status" data-connected="true" data-port="MI"'),
           "the Launchpad status names the MI port");
    check (audioHtml.includes ('data-testid="launchpad-port" data-port="MI">MI<'),
           "the Launchpad port pill reads MI");
    check (audioHtml.includes ('data-testid="launchpad-handshake" data-handshake="true"'),
           "the Launchpad programming handshake is shown");
    check (audioHtml.includes ('data-testid="keystation-status" data-connected="false" data-port="USB"'),
           "the Keystation is detected on its USB port");
    check (audioHtml.includes ('data-testid="pad-test-result" data-active="false" data-lit="0"'),
           "the pad test has not run yet");
}
