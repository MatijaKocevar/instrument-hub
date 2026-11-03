import { createControlApi } from "../../api/client";
import { instrumentSlotForRoute } from "../../features/instrument";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { check, renderApp, writeArtifact } from "../harness";
import { assertSnapshotAudio, assertSnapshotAudioChannels } from "./audio";
import {
  assertSnapshotArmed,
  assertSnapshotGrid,
  assertSnapshotGridFixture,
} from "./gridSnapshot";
import { assertSnapshotInstrument } from "./instrument";
import { assertSnapshotMidi } from "./midi";
import { assertSnapshotPairing, assertSnapshotPairingMoved } from "./pairing";
import { assertSnapshotPlugins, assertSnapshotPluginsFixture } from "./plugins";
import {
  assertSnapshotActiveNav,
  assertSnapshotFooter,
  assertSnapshotNonBlank,
  assertSnapshotShell,
} from "./shell";

export async function snapshotTest(): Promise<void> {
  const externalUrl = process.argv
    .find((argument) => argument.startsWith("--api="))
    ?.slice("--api=".length);

  if (externalUrl !== undefined && externalUrl.length > 0) {
    console.log(`snapshot: rendering state from live API ${externalUrl}`);

    await renderSnapshot(externalUrl, false);

    return;
  }

  const server = await startFixtureServer();

  try {
    await renderSnapshot(server.baseUrl, true);
    check(server.stateRequests === 1, "exactly one state request was made");
  } finally {
    await server.close();
  }
}

async function renderSnapshot(
  baseUrl: string,
  fromFixture: boolean,
): Promise<void> {
  const store = new AppStore(createControlApi({ baseUrl }));
  const loaded = await store.refresh();

  check(loaded, "GET /state succeeds over HTTP");

  const armedSlot = store.getSnapshot().state?.armedSlot ?? -1;
  const instrumentSlot = instrumentSlotForRoute("instrument", 3, armedSlot);

  if (instrumentSlot !== null) await store.loadFxChain(instrumentSlot);

  await store.loadPlugins();

  const inventory = store.getSnapshot().plugins;

  const html = renderApp(store);
  const audioHtml = renderApp(store, "/audio");
  const instrumentHtml = renderApp(store, "/instrument?slot=3");
  const pluginsHtml = renderApp(store, "/plugins");
  const settingsHtml = renderApp(store, "/settings");

  const suffix = fromFixture ? "snapshot" : "live";

  writeArtifact(`ui-state-${suffix}.html`, html);
  writeArtifact(`ui-audio-${suffix}.html`, audioHtml);
  writeArtifact(`ui-instrument-${suffix}.html`, instrumentHtml);
  writeArtifact(`ui-plugins-${suffix}.html`, pluginsHtml);
  writeArtifact(`ui-settings-${suffix}.html`, settingsHtml);

  assertSnapshotNonBlank(html);
  assertSnapshotGrid(html);
  assertSnapshotShell(html);
  assertSnapshotAudio(audioHtml);
  assertSnapshotMidi(audioHtml, fromFixture);
  assertSnapshotPairing(settingsHtml);
  assertSnapshotPairingMoved(audioHtml);
  assertSnapshotActiveNav(audioHtml, settingsHtml);
  assertSnapshotAudioChannels(audioHtml, store);
  assertSnapshotInstrument(instrumentHtml);
  assertSnapshotPlugins(pluginsHtml, inventory);
  assertSnapshotArmed(html, armedSlot);
  assertSnapshotFooter(html, armedSlot);

  if (fromFixture) {
    assertSnapshotGridFixture(html);
    assertSnapshotPluginsFixture(pluginsHtml);
  }
}
