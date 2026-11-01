// Pairing: QR encoding, the pairing gate and LAN-mode lifecycle.

import { renderToStaticMarkup } from "react-dom/server";

import { ApiError, createControlApi } from "../../api/client";
import {
    PairingGate,
    codeFromSearch,
    encodeQr,
    loadPairingToken,
    qrPathData,
} from "../../features/pairing";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { PAIRING_ON } from "../fixtures";
import { check, renderApp } from "../harness";
import { MemoryStorage } from "../memoryStorage";

/**
    Reference matrix for

        http://instrument-hub.local:8080/remote?code=987654

    produced with `qrencode -t ASCII -v 4 -l M -m 0 -8` (mask 0, ECC M).
    Comparing against it proves the encoder's mode/count fields, Reed–Solomon
    blocks, interleaving, function patterns and format bits are all correct.
*/
const QR_GOLDEN = "111111100011100110111111001111111/100000101100110101010111101000001/101110100011000010011000001011101/101110100111010001010001101011101/101110101001100001000101101011101/100000100011010000100111101000001/111111101010101010101010101111111/000000000110010011001111100000000/101010100101111101011100000010010/011001001000001000001000111000001/111001111100110011000010111100111/011111010010100000011110000010001/101001101101100100001110101001010/110101001111110100010110101001001/010110100110001000001000100010011/110101000101101101011111100000010/011110100011010001000100011000000/110101011100011001001000111100011/101101101011100010101010110100101/000101010110110111110101101010010/111101110000101110110110111101001/010111000000010110100010111000011/100110100110110111000100010101111/011101011010100011111100111010001/101110110111001111001101111110001/000000001000101010001110100011001/111111100101101001000101101010101/100000100001110100011110100011010/101110101011101000010110111111000/101110100000000101000110100110101/101110101001101001101000110011101/100000100001000101101101110110010/111111101101011011001111110000011";

export function qrTest (): void
{
    const golden = QR_GOLDEN.split ("/");
    const code = encodeQr ("http://instrument-hub.local:8080/remote?code=987654", { mask: 0 });

    check (code.size === 33 && code.version === 4, "QR is version 4 (33×33)");
    check (code.modules.map (row => row.map (value => value ? "1" : "0").join ("")).join ("/")
               === golden.join ("/"),
           "QR matrix matches the qrencode reference (mode, RS, interleave, format)");

    const path = qrPathData (code);
    check (path.length > 1000 && path.startsWith ("M0 0h1v1h-1z"),
           "QR path starts with the top-left finder module");

    const fixture = encodeQr (PAIRING_ON.payload);
    check (fixture.mask >= 0 && fixture.mask <= 7, "QR selects a valid mask");
    check (fixture.modules[0]!.slice (0, 7).every (Boolean)
               && fixture.modules[0]![7] === false,
           "top-left finder pattern is present");
    check (fixture.modules[fixture.size - 7]!.slice (0, 7).every (Boolean)
               && fixture.modules[fixture.size - 1]![0] === true,
           "bottom-left finder pattern is present");
    check (fixture.modules[0]!.slice (fixture.size - 7).every (Boolean),
           "top-right finder pattern is present");

    let tooLong = false;

    try
    {
        encodeQr ("x".repeat (63));
    }
    catch
    {
        tooLong = true;
    }

    check (tooLong, "a payload beyond version 4's capacity is rejected");

    check (codeFromSearch ("?code=123456") === "123456", "the QR link supplies the code");
    check (codeFromSearch ("?code=12x456") === null, "a malformed link code is ignored");
    check (codeFromSearch ("?other=1") === null, "a link without a code is ignored");
}

export async function pairingTest (): Promise<void>
{
    const storage = new MemoryStorage ();
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { storage, pollIntervalMs: 60_000 });

        await store.refresh ();

        check (store.getSnapshot ().pairing?.lanEnabled === false,
               "the desktop reads LAN mode off from GET /pairing");
        check (store.token === null, "a fresh desktop has no pairing token");

        const before = renderApp (store, "/settings");

        check (before.includes ('data-testid="pairing" data-lan="false"'),
               "pairing card renders with LAN mode off");
        check (! before.includes ('data-testid="pairing-qr"'),
               "no QR is rendered before LAN mode is enabled");

        const enabled = await store.toggleLan (true);

        check (enabled, "turning LAN mode on succeeds");
        check (store.getSnapshot ().pairing?.lanEnabled === true,
               "the store adopts the LAN-on pairing state");
        check (store.token === "fixture-pairing-token",
               "the desktop pairs itself with the issued code");
        check (server.lanRequests.at (-1) === true, "POST /pairing/lan received enabled: true");
        check (server.pairRequests.includes ("246810"),
               "the desktop redeemed the first six-digit code");
        check (loadPairingToken (storage) === "fixture-pairing-token",
               "the pairing token is stored client-side");

        const after = renderApp (store, "/settings");

        check (after.includes ('data-testid="pairing" data-lan="true"'),
               "pairing card reports LAN mode on");
        check (after.includes ('data-testid="lan-toggle" aria-pressed="true"'),
               "the LAN toggle is pressed after enabling");
        check (after.includes ('data-testid="pairing-code" data-code="135790"'),
               "the rotated six-digit code reaches the panel");
        check (after.includes ("135 790"), "the code is readable in three-digit groups");
        check (after.includes ('data-testid="pairing-qr"')
                   && after.includes ('data-payload="http://192.168.1.20:8080/remote?code=135790"'),
               "the QR carries the live remote payload");
        check (after.includes ('data-version="4"'), "the QR identifies its version");
        check (after.includes ('data-testid="pairing-expiry">5:00<'),
               "the panel shows the code lifetime");
        check (after.includes ('data-testid="pairing-clients">1<'),
               "the panel counts the paired desktop");
        check (after.includes ('data-testid="pairing-token-state" data-paired="true"'),
               "the desktop reports itself as paired");
        check (after.includes ('data-testid="settings-token-state" data-paired="true"'),
               "the account card reports this browser's stored token");
        check (after.includes ('data-testid="settings-revoke-token"')
                   && ! /data-testid="settings-revoke-token"[^>]*disabled/.test (after),
               "token revocation is available while LAN mode is on");

        // --- The acceptance flow, through a brand-new client --------------
        const anonymous = createControlApi ({ baseUrl: server.baseUrl });
        let unpairedStatus = 0;

        try
        {
            await anonymous.getState ();
        }
        catch (error)
        {
            unpairedStatus = error instanceof ApiError ? error.status : 0;
        }

        check (unpairedStatus === 401, "an unpaired client gets 401");

        let wrongCodeStatus = 0;

        try
        {
            await anonymous.pair ("000000");
        }
        catch (error)
        {
            wrongCodeStatus = error instanceof ApiError ? error.status : 0;
        }

        check (wrongCodeStatus === 401, "a wrong six-digit code is rejected");

        const paired = await anonymous.pair (server.pairing.code);

        check (paired.token === "fixture-pairing-token", "pairing with the code returns a token");

        anonymous.setToken (paired.token);

        const state = await anonymous.getState ();

        check (state.armedSlot === 3, "the token grants GET /state");

        // --- Tablet gate ---------------------------------------------------
        const gate = renderToStaticMarkup (
            <PairingGate pairing={server.pairing} initialCode="246810" onPair={() => {}} />);

        check (gate.includes ('data-testid="pairing-gate"'), "the remote gate renders");
        check (gate.includes ('data-testid="pairing-code-input"')
                   && gate.includes ('value="246810"'),
               "the gate pre-fills the scanned code");
        check (gate.includes ('data-testid="pairing-submit"'), "the gate has a pair button");
        check (gate.includes ("stays in this browser"), "the gate explains token storage");

        // --- Forget / turn LAN mode off -----------------------------------
        check (await store.forgetPairing (), "forgetting paired tablets succeeds");
        check (store.getSnapshot ().pairing?.pairedClients === 1,
               "forget revokes others and re-pairs this desktop");
        check (loadPairingToken (storage) === "fixture-pairing-token",
               "forgetting keeps this browser's fresh token");

        check (await store.toggleLan (false), "turning LAN mode off succeeds");
        check (store.getSnapshot ().pairing?.lanEnabled === false,
               "the store adopts the LAN-off pairing state");
        check (store.token === null, "turning LAN mode off drops the local token");
        check (loadPairingToken (storage) === null, "the stored token is removed");

        const off = renderApp (store, "/settings");

        check (off.includes ('data-testid="pairing" data-lan="false"'),
               "the card returns to the LAN-off state");
        check (! off.includes ('data-testid="pairing-qr"'),
               "the QR disappears when LAN mode is off");
    }
    finally
    {
        await server.close ();
    }
}

/** Full-app snapshot: the settings page pairs desktop and tablets. */
export function assertSnapshotPairing (settingsHtml: string): void
{
    check (settingsHtml.includes ('data-testid="settings-page"'), "the /settings route resolves");
    check (settingsHtml.includes ('data-testid="pairing"'), "settings shows the tablet pairing card");
    check (settingsHtml.includes ('data-testid="pairing" data-lan="false"'),
           "LAN mode is off by default");
    check (settingsHtml.includes ('data-testid="lan-toggle" aria-pressed="false"'),
           "the LAN toggle starts off");
    check (! settingsHtml.includes ('data-testid="pairing-qr"'),
           "no QR code is rendered while LAN mode is off");
    check (settingsHtml.includes ('data-testid="pairing-token-state" data-paired="false"'),
           "an unpaired desktop reports no token");
    check (settingsHtml.includes ('data-testid="settings-account"'),
           "settings exposes the account section");
    check (settingsHtml.includes ('data-testid="settings-revoke-token"'),
           "settings exposes token revocation");
    check (settingsHtml.includes ('data-testid="settings-about"'),
           "settings exposes an About section");
    check (settingsHtml.includes ('data-testid="settings-version">v0.1.0<'),
           "the About section shows the engine version");
}

/** Full-app snapshot: pairing lives on Settings, not on Audio & MIDI. */
export function assertSnapshotPairingMoved (audioHtml: string): void
{
    check (! audioHtml.includes ('data-testid="pairing"'),
           "pairing no longer lives on the Audio & MIDI page");
}
