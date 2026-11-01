import { renderToStaticMarkup } from "react-dom/server";

import {
  createControlApi,
  type ControlFxChain,
  type ControlPatch,
  type ControlPluginList,
} from "../../api/client";
import {
  FADER_MIN_DB,
  InstrumentPage,
  chainWithBypass,
  chainWithMove,
  chainWithoutInsert,
  clampPan,
  effectPlugins,
  followsArmedSlot,
  formatDb,
  formatPan,
  gainFromDb,
  insertRequestForBuiltin,
  insertRequestForPlugin,
  instrumentPlugins,
  instrumentSlotForRoute,
  isPluginSource,
  moveOrder,
  sourceKindLabel,
  sourceRequestForInitTone,
  sourceRequestForPlugin,
  wetPercent,
} from "../../features/instrument";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { PLUGIN_FIXTURE } from "../fixtures";
import { check, renderApp, writeArtifact } from "../harness";

export function instrumentModelTest(): void {
  check(gainFromDb(0) === 1, "0 dB is unity gain");
  check(Math.abs(gainFromDb(-20) - 0.1) < 0.0001, "-20 dB is 0.1 linear gain");
  check(gainFromDb(FADER_MIN_DB) === 0, "the fader floor is silence");
  check(
    formatDb(0) === "0.0 dB" && formatDb(-18) === "-18.0 dB",
    "dB readout is fixed width",
  );
  check(
    formatPan(-0.4) === "L 40" &&
      formatPan(0) === "C" &&
      formatPan(0.25) === "R 25",
    "balance reads as L/C/R with a percentage",
  );
  check(clampPan(2) === 1 && clampPan(-2) === -1, "balance is clamped to ±1");
  check(wetPercent(0.355) === 36, "wet is a whole percentage");

  check(
    moveOrder(3, 2, 0).join(",") === "2,0,1",
    "moving the last insert to the front builds a permutation",
  );
  check(
    moveOrder(3, 0, 2).join(",") === "1,2,0",
    "moving the first insert to the end builds a permutation",
  );
  check(
    moveOrder(3, -5, 99).join(",") === "1,2,0",
    "out-of-range moves are clamped",
  );
  check(moveOrder(0, 0, 0).length === 0, "an empty chain has no order");

  const chain: ControlFxChain = {
    slot: 3,
    instrumentLatencySamples: 84,
    fxLatencySamples: 156,
    latencySamples: 240,
    latencyMs: 5,
    inserts: [
      {
        index: 0,
        type: "eq",
        kind: "builtin",
        id: "eq",
        name: "EQ",
        bypassed: false,
        wet: 1,
        dry: 0,
        latencySamples: 0,
        parameters: {},
      },
      {
        index: 1,
        type: "compressor",
        kind: "builtin",
        id: "compressor",
        name: "Compressor",
        bypassed: false,
        wet: 1,
        dry: 0,
        latencySamples: 156,
        parameters: {},
      },
    ],
  };

  const bypassed = chainWithBypass(chain, 1, true);

  check(
    bypassed.fxLatencySamples === 0 && bypassed.latencySamples === 84,
    "bypassing an insert drops its latency from the budget",
  );
  check(
    bypassed.latencyMs === 84 * (5 / 240),
    "the millisecond readout follows the sample total",
  );

  const removed = chainWithoutInsert(chain, 0);

  check(
    removed.inserts.length === 1 && removed.inserts[0]?.index === 0,
    "removing the first insert renumbers the chain",
  );

  const moved = chainWithMove(chain, 1, 0);

  check(
    moved.inserts[0]?.type === "compressor" && moved.inserts[1]?.index === 1,
    "moving an insert reorders the chain and renumbers it",
  );

  const plugins: ControlPluginList = PLUGIN_FIXTURE;

  check(
    instrumentPlugins(plugins.plugins).length === 2 &&
      instrumentPlugins(plugins.plugins).every((plugin) => plugin.isInstrument),
    "only instrument plug-ins are offered as a source",
  );
  check(
    effectPlugins(plugins.plugins).length === 1 &&
      effectPlugins(plugins.plugins)[0]?.name === "TestReverb",
    "only effect plug-ins are offered as an insert",
  );

  const source = sourceRequestForPlugin(plugins.plugins[0]!);

  check(
    source.kind === "plugin" && source.path === "/usr/lib/vst3/TestPlugin.vst3",
    "a plug-in source request carries the bundle path",
  );
  check(
    source.pluginManufacturer === "Instrument Hub" &&
      source.pluginVersion === "1.0.0",
    "the plug-in identity reaches the source request",
  );
  check(
    sourceRequestForInitTone().kind === "init-tone",
    "the built-in tone is a source request",
  );
  check(
    insertRequestForBuiltin("delay").type === "delay",
    "a built-in insert is addressable by type",
  );
  check(
    insertRequestForPlugin(plugins.plugins[1]!).type === "vst3",
    "a plug-in insert uses the vst3 type",
  );

  check(
    instrumentSlotForRoute("instrument", null, 3) === 3,
    "the instrument page follows the armed slot",
  );
  check(
    instrumentSlotForRoute("instrument", 1, 3) === 3,
    "the controller selection beats a stale ?slot link",
  );
  check(
    instrumentSlotForRoute("instrument", 1, -1) === 1,
    "a link still selects a patch while nothing is armed",
  );
  check(
    instrumentSlotForRoute("grid", null, 3) === null,
    "other routes never resolve to an instrument slot",
  );
  check(
    followsArmedSlot(3) && !followsArmedSlot(-1),
    "the page follows the controller whenever a patch is armed",
  );
  check(isPluginSource(null) === false, "no patch has no plug-in source");
  check(
    sourceKindLabel("plugin") === "VST3 plug-in",
    "the source kind has a human label",
  );
}

export async function instrumentPageTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    check(await store.refresh(), "the instrument fixture loads GET /state");
    check(
      await store.loadFxChain(3),
      "GET /patches/3/fx returns the insert chain",
    );
    check(
      await store.loadPlugins(),
      "GET /plugins returns the installed inventory",
    );
    check(
      server.pluginRequests.length === 1,
      "the page fetched the inventory once",
    );

    const initial = renderApp(store, "/instrument?slot=3");

    writeArtifact("ui-instrument-snapshot.html", initial);

    check(
      initial.includes('data-testid="instrument-page" data-slot="3"'),
      "the instrument page renders the queried slot",
    );
    check(
      initial.includes('data-testid="instrument-name">Drawbar Organ<'),
      "the page shows the patch's real name",
    );
    check(
      initial.includes("Keys · slot 04 · armed · following the controller"),
      "the page shows family, slot and arm state",
    );
    check(
      initial.includes(
        'data-testid="instrument-warm-state" data-warm="true" data-warm-state="active"',
      ),
      "the warm state comes from /state",
    );
    check(
      initial.includes(
        'data-testid="instrument-latency" data-latency-samples="240" data-latency-ms="5.000"',
      ),
      "the chain latency comes from /state",
    );
    check(
      initial.includes("5.00 ms · 240 smp"),
      "latency is readable in ms and samples",
    );
    check(
      initial.includes(
        'data-testid="volume-fader" data-slot="3" data-db="0.00"',
      ),
      "the fader starts at the patch's real volume",
    );
    check(
      initial.includes('data-testid="volume-value">0.0 dB<'),
      "the fader has a dB readout",
    );
    check(
      initial.includes('data-testid="volume-gain" data-gain="1.000000"'),
      "the page publishes the effective linear gain",
    );
    check(
      initial.includes('data-testid="pan-value">C<'),
      "the balance starts centred",
    );
    check(
      initial.includes(
        'data-testid="mute-button" data-slot="3" data-muted="false"',
      ),
      "mute reflects the patch",
    );
    check(
      initial.includes(
        'data-testid="solo-button" data-slot="3" data-solo="false"',
      ),
      "solo reflects the patch",
    );
    check(
      initial.includes('data-testid="instrument-colour" data-colour-index="0"'),
      "the patch colour reaches the page",
    );

    check(
      initial.includes('data-testid="instrument-source" data-kind="init-tone"'),
      "the page names the declared instrument source",
    );
    check(
      initial.includes('data-testid="source-open-editor"'),
      "the source has an editor control",
    );
    check(
      initial.includes('data-testid="source-save-state"'),
      "the page offers an explicit save-sound action",
    );
    check(
      /data-testid="source-save-state"[^>]*disabled/.test(initial),
      "a generated source cannot save a plug-in sound",
    );
    check(
      initial.includes("Only plug-in sources have a sound state to save"),
      "the disabled save explains why",
    );
    check(
      initial.includes('data-testid="source-change"'),
      "the source can be changed",
    );
    check(
      initial.includes('value="/usr/lib/vst3/TestPlugin.vst3"'),
      "the source picker lists installed instrument plug-ins",
    );
    check(
      !initial.includes('value="/usr/lib/vst3/TestReverb.vst3"'),
      "the source picker excludes effect plug-ins",
    );

    check(
      initial.includes('data-testid="fx-chain" data-slot="3" data-count="2"'),
      "the insert chain renders both inserts",
    );
    check(
      initial.includes('data-latency-samples="240" data-latency-ms="5.000"'),
      "the chain publishes its latency budget",
    );
    check(
      initial.includes('data-testid="fx-latency-instrument">84 smp<'),
      "the chain separates instrument latency",
    );
    check(
      initial.includes('data-testid="fx-latency-inserts">156 smp<'),
      "the chain separates insert latency",
    );
    check(
      initial.includes('data-testid="fx-insert" data-index="0" data-type="eq"'),
      "the first insert is the EQ",
    );
    check(
      initial.includes(
        'data-testid="fx-insert" data-index="1" data-type="compressor"',
      ),
      "the second insert is the compressor",
    );
    check(
      initial.includes('data-testid="fx-bypass"') &&
        initial.includes('title="Bypass EQ"') &&
        initial.includes('aria-pressed="true"'),
      "an insert starts enabled with a bypass control",
    );
    check(
      initial.includes('value="builtin:eq"') &&
        initial.includes('value="plugin:com.instrumenthub.TestReverb"'),
      "the add control offers built-ins and installed effects",
    );

    check(
      await store.updatePatch(3, { volumeDb: -18 }),
      "moving the fader succeeds",
    );
    check(
      server.patchRequests.length === 1 &&
        server.patchRequests[0]?.slot === 3 &&
        server.patchRequests[0]?.patch.volumeDb === -18,
      "the fader sends volumeDb: -18 to PATCH /patches/3",
    );

    const quiet = renderApp(store, "/instrument?slot=3");

    check(
      quiet.includes(
        'data-testid="volume-fader" data-slot="3" data-db="-18.00"',
      ),
      "the fader shows the new level",
    );
    check(
      quiet.includes('data-testid="volume-gain" data-gain="0.125893"'),
      "-18 dB resolves to 0.1259 linear gain",
    );
    check(
      quiet.includes('data-volume-db="-18.00" data-gain="0.125893"'),
      "the mixer card publishes the level the engine applies",
    );

    check(await store.updatePatch(3, { mute: true }), "muting succeeds");
    check(
      server.patchRequests.at(-1)?.patch.mute === true,
      "mute reaches the engine",
    );
    check(
      renderApp(store, "/instrument?slot=3").includes(
        'data-testid="mute-button" data-slot="3" data-muted="true"',
      ),
      "the mute button reflects the change",
    );

    check(await store.updatePatch(3, { solo: true }), "soloing succeeds");
    check(
      renderApp(store, "/instrument?slot=3").includes(
        'data-testid="solo-button" data-slot="3" data-solo="true"',
      ),
      "the solo button reflects the change",
    );

    check(
      await store.updatePatch(3, { colorIndex: 7 }),
      "recolouring succeeds",
    );
    check(
      renderApp(store, "/instrument?slot=3").includes(
        'data-testid="instrument-colour" data-colour-index="7"',
      ),
      "the colour swatch follows the patch",
    );

    check(
      await store.addFxInsert(3, insertRequestForBuiltin("compressor")),
      "adding a compressor succeeds",
    );
    check(
      server.chain.inserts.length === 3 && server.chain.latencySamples === 396,
      "the engine added the insert and summed its latency",
    );

    const added = renderApp(store, "/instrument?slot=3");

    check(
      added.includes('data-testid="fx-chain" data-slot="3" data-count="3"'),
      "the new insert appears in the chain",
    );
    check(
      added.includes('data-latency-samples="396"'),
      "the chain latency grows with the insert",
    );

    check(
      await store.moveFxInsert(3, 2, 0),
      "moving the last insert to the front succeeds",
    );
    check(
      server.fxRequests.at(-1)?.method === "POST" &&
        (server.fxRequests.at(-1)?.body.order as number[]).join(",") ===
          "2,0,1",
      "reordering sends a permutation to /fx/reorder",
    );
    check(
      server.chain.inserts[0]?.index === 0 &&
        server.chain.inserts[0]?.type === "compressor",
      "the engine applied the new order",
    );

    const moved = renderApp(store, "/instrument?slot=3");

    check(
      /data-testid="fx-insert" data-index="0" data-type="compressor"/.test(
        moved,
      ),
      "the page renders the reordered chain",
    );

    check(
      await store.updateFxInsert(3, 0, { bypassed: true }),
      "bypassing succeeds",
    );
    check(
      server.fxRequests.at(-1)?.method === "PATCH" &&
        server.fxRequests.at(-1)?.body.bypassed === true,
      "bypass reaches PATCH /patches/3/fx/0",
    );
    check(
      server.chain.latencySamples === 240,
      "a bypassed insert leaves the latency budget",
    );

    const bypassed = renderApp(store, "/instrument?slot=3");

    check(
      bypassed.includes('data-count="3" data-latency-samples="240"'),
      "the page drops the bypassed insert's latency",
    );
    check(
      /data-index="0" data-type="compressor" data-kind="builtin" data-name="compressor" data-bypassed="true"/.test(
        bypassed,
      ),
      "the bypassed row is marked",
    );

    check(await store.removeFxInsert(3, 1), "removing an insert succeeds");
    check(
      server.fxRequests.at(-1)?.method === "DELETE" &&
        server.fxRequests.at(-1)?.index === 1,
      "remove reaches DELETE /patches/3/fx/1",
    );
    check(server.chain.inserts.length === 2, "the engine dropped the insert");

    const removed = renderApp(store, "/instrument?slot=3");

    check(
      removed.includes('data-count="2"'),
      "the page renders the shortened chain",
    );
    check(
      !removed.includes('data-type="eq"'),
      "the removed insert is gone from the page",
    );

    const plugin = PLUGIN_FIXTURE.plugins[0]!;

    check(
      await store.changeInstrumentSource(3, sourceRequestForPlugin(plugin)),
      "assigning an installed plug-in as the source succeeds",
    );
    check(
      server.sourceRequests.length === 1 &&
        server.sourceRequests[0]?.slot === 3 &&
        server.sourceRequests[0]?.body.kind === "plugin" &&
        server.sourceRequests[0]?.body.path === "/usr/lib/vst3/TestPlugin.vst3",
      "POST /patches/3/source carries the bundle path",
    );

    const pluginPage = renderApp(store, "/instrument?slot=3");

    check(
      pluginPage.includes('data-testid="instrument-source" data-kind="plugin"'),
      "the page shows the new source kind",
    );
    check(
      pluginPage.includes('data-name="TestPlugin"'),
      "the page shows the plug-in name",
    );
    check(
      pluginPage.includes("native window on the engine desktop"),
      "a plug-in source offers its native window",
    );
    check(
      !pluginPage.includes("hosted headlessly"),
      "the page no longer claims plug-ins are hosted headlessly",
    );
    check(
      /data-testid="source-save-state"[^>]*data-slot="3"/.test(pluginPage) &&
        !/data-testid="source-save-state"[^>]*disabled/.test(pluginPage),
      "a plug-in source offers an enabled save-sound action",
    );

    check(
      await store.savePatchSound(3),
      "the explicit Save sound to patch action calls the API",
    );
    check(
      server.stateCaptureRequests.at(-1)?.slot === 3,
      "POST /patches/3/state/capture saves the running sound",
    );
    check(
      store.getSnapshot().capture?.sizeBytes === 4099,
      "the store keeps the engine's capture confirmation",
    );

    const savedPage = renderApp(store, "/instrument?slot=3");

    check(
      savedPage.includes('data-testid="source-save-status"') &&
        savedPage.includes("Saved to patch · 4.0 kB"),
      "the page confirms the saved patch in place",
    );
    check(
      savedPage.includes('data-size-bytes="4099"'),
      "the confirmation publishes the stored blob size",
    );

    check(
      await store.openInstrumentEditor(3),
      "the explicit Open editor action calls the API",
    );
    check(
      server.editorRequests.at(-1)?.slot === 3 &&
        server.editorRequests.at(-1)?.open === true,
      "POST /patches/3/editor opens the window",
    );

    const openedPage = renderApp(store, "/instrument?slot=3");

    check(
      openedPage.includes(
        'data-testid="source-editor" data-kind="plugin" data-open="true"',
      ),
      "the page reflects the open editor",
    );
    check(
      openedPage.includes('data-testid="source-close-editor"'),
      "an open editor offers a close control",
    );

    check(
      await store.closeInstrumentEditor(3),
      "the explicit close action calls the API",
    );
    check(
      server.editorRequests.at(-1)?.slot === 3 &&
        server.editorRequests.at(-1)?.open === false,
      "DELETE /patches/3/editor closes the window",
    );

    const closedPage = renderApp(store, "/instrument?slot=3");

    check(
      closedPage.includes(
        'data-testid="source-editor" data-kind="plugin" data-open="false"',
      ),
      "the page reflects the closed editor",
    );

    check(
      await store.changeInstrumentSource(3, sourceRequestForInitTone(880)),
      "switching back to the built-in tone succeeds",
    );
    check(
      server.sourceRequests.at(-1)?.body.frequency === 880,
      "the tone frequency reaches the engine",
    );

    const tonePage = renderApp(store, "/instrument?slot=3");

    check(
      tonePage.includes(
        'data-testid="instrument-source" data-kind="init-tone"',
      ),
      "the page returns to the tone source",
    );
    check(
      tonePage.includes("there are no plug-in parameters to edit"),
      "a generated source explains that it has no editor parameters",
    );

    const following = renderApp(store, "/instrument");

    check(
      following.includes(
        'data-testid="instrument-page" data-slot="3" data-armed="true" data-following="true"',
      ),
      "the instrument page follows the armed slot",
    );
    check(
      following.includes("following the controller"),
      "the page says it is following the controller",
    );

    check(await store.armSlot(5), "the controller arms another patch");
    check(
      renderApp(store, "/instrument").includes(
        'data-testid="instrument-page" data-slot="5"',
      ),
      "the page follows the new armed slot",
    );
    check(
      renderApp(store, "/instrument?slot=1").includes(
        'data-testid="instrument-page" data-slot="5"',
      ),
      "a stale ?slot link does not override the controller",
    );

    check(await store.armSlot(7), "the controller arms a third patch");
    check(
      renderApp(store, "/instrument?slot=1").includes(
        'data-testid="instrument-page" data-slot="7"',
      ),
      "the page keeps following later arms",
    );
    check(
      !renderApp(store, "/grid").includes('data-testid="instrument-page"'),
      "arming never navigates the grid to the instrument page",
    );
    check(
      !renderApp(store, "/plugins").includes('data-testid="instrument-page"'),
      "arming never navigates the plugins page to the instrument page",
    );

    const failing = new AppStore(
      createControlApi({ baseUrl: "http://127.0.0.1:1" }),
      { pollIntervalMs: 60_000 },
    );

    check(!(await failing.loadFxChain(3)), "a chain failure is reported");
    check(
      failing.getSnapshot().fxError !== null,
      "the page can show the chain error",
    );
    check(
      !(await failing.savePatchSound(3)),
      "a save-sound failure is reported",
    );
    check(
      failing.getSnapshot().captureError !== null,
      "the page can show the save-sound error",
    );
  } finally {
    await server.close();
  }
}

export function instrumentPluginEditorTest(): void {
  const pluginPatch: ControlPatch = {
    slot: 2,
    id: "plugin-patch",
    name: "Plugin Patch",
    family: "keys",
    familyName: "Keys",
    instrument: {
      id: "com.instrumenthub.TestPlugin",
      name: "TestPlugin",
      kind: "plugin",
      pluginFormat: "VST3",
      pluginId: "com.instrumenthub.TestPlugin",
      pluginName: "TestPlugin",
      pluginManufacturer: "Instrument Hub",
      pluginVersion: "1.0.0",
    },
    volumeDb: -6,
    pan: -0.5,
    mute: false,
    solo: false,
    colorIndex: 4,
    warmState: "warm",
    warm: true,
    editorOpen: false,
    latencySamples: 96,
    latencyMs: 2,
  };

  const html = renderToStaticMarkup(
    <InstrumentPage
      patch={pluginPatch}
      armedSlot={2}
      following={false}
      chain={null}
      plugins={PLUGIN_FIXTURE.plugins}
      onCaptureState={() => {}}
    />,
  );

  check(
    html.includes('data-testid="instrument-source" data-kind="plugin"'),
    "a plugin patch renders its source",
  );
  check(
    html.includes('data-testid="source-open-editor"'),
    "a plugin patch offers its editor",
  );
  check(
    html.includes('data-testid="source-save-state"'),
    "a plugin patch offers an explicit sound save",
  );
  check(
    !/data-testid="source-save-state"[^>]*disabled/.test(html),
    "the plugin sound save is enabled",
  );
  check(
    html.includes("native window on the engine desktop"),
    "the plugin editor offers the native window",
  );
  check(
    html.includes('data-testid="fx-loading"') &&
      html.includes('aria-busy="true"'),
    "a chain that has not loaded shows its loading state",
  );
  check(
    html.includes('data-testid="pan-value">L 50<'),
    "a left balance reads L 50",
  );
  check(
    html.includes('data-testid="fx-latency-total">latency unavailable<'),
    "a missing chain reports no latency instead of a zero",
  );

  const empty = renderApp(
    new AppStore(createControlApi({ baseUrl: "http://127.0.0.1:1" })),
    "/instrument",
  );

  check(
    empty.includes('data-testid="instrument-page" data-slot="-1"'),
    "the instrument page renders its empty state before the first state",
  );
  check(
    empty.includes('data-testid="instrument-name">Loading the set…<'),
    "the empty state explains that the set is loading",
  );
}

export function assertSnapshotInstrument(instrumentHtml: string): void {
  check(
    instrumentHtml.includes('data-testid="instrument-page" data-slot="3"'),
    "the /instrument route resolves to the armed patch",
  );
  check(
    instrumentHtml.includes('data-testid="instrument-name">Drawbar Organ<'),
    "the instrument page shows the patch at the queried slot",
  );
}
