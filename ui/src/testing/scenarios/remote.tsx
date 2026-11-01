import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi } from "../../api/client";
import { MeterStore } from "../../features/grid";
import {
  DEFAULT_REMOTE_CONTRAST,
  INITIAL_LINK,
  MeterLink,
  RemoteView,
  WakeLockController,
  formatPosition,
  isRemotePath,
  loadRemoteContrast,
  nextContrast,
  remoteBanner,
  remoteFromState,
  saveRemoteContrast,
  stepPatch,
  type RemoteLinkSnapshot,
  type WakeLockCapableNavigator,
  type WakeLockSentinelLike,
  type WakeLockStatus,
} from "../../features/remote";
import { AppStore } from "../../state/store";
import { FakeMeterSocket } from "../fakes/meterSocket";
import { startFixtureServer } from "../fixtureServer";
import { FIXTURE } from "../fixtures";
import { check, countOf, waitFor, writeArtifact } from "../harness";
import { MemoryStorage } from "../memoryStorage";

export function remoteSnapshotTest(): void {
  const model = remoteFromState(FIXTURE);
  const html = renderToStaticMarkup(
    <RemoteView model={model} connection="ready" />,
  );

  writeArtifact("ui-remote-snapshot.html", html);

  check(html.length > 2000, "remote snapshot is non-blank");
  check(html.includes('data-testid="remote"'), "remote renders its root");
  check(
    html.includes('data-testid="remote-set-count">8 patches<'),
    "remote set list reports eight patches",
  );
  check(
    countOf(html, 'data-testid="remote-set-item"') === 8,
    "remote renders one large target per patch",
  );
  check(
    countOf(html, 'data-armed="true"') === 1,
    "exactly one remote patch is armed",
  );
  check(
    html.includes(
      'data-testid="remote-set-item" data-slot="3" data-armed="true"',
    ),
    "the remote highlights the armed slot",
  );
  check(
    html.includes('data-testid="remote-current-name">Sub Bass<'),
    "the now-playing card shows the armed patch",
  );
  check(
    html.includes("chain 5.0 ms"),
    "the now-playing card shows chain latency",
  );
  check(
    html.includes('data-testid="remote-prev"'),
    "remote has a previous-patch control",
  );
  check(
    html.includes('data-testid="remote-next"'),
    "remote has a next-patch control",
  );
  check(
    html.includes('data-testid="remote-play"'),
    "remote has a play/pause control",
  );
  check(
    html.includes('data-testid="remote-stop"'),
    "remote has a transport stop control",
  );
  check(
    html.includes('data-testid="remote-panic"'),
    "remote has a panic control",
  );
  check(
    html.includes('data-testid="remote-bpm">128 BPM<'),
    "remote shows the fetched tempo",
  );
  check(
    html.includes('data-testid="remote-transport-state">Playing<'),
    "remote shows the fetched transport state",
  );
  check(
    html.includes('data-playing="true"'),
    "remote root reports playing state",
  );
  check(
    html.includes('data-testid="remote-position">Bar 5.1'),
    "remote shows the musical position",
  );
  check(html.includes('data-theme="stage"'), "remote uses the stage theme");
  check(
    html.includes('data-contrast="high"'),
    "the stage remote starts in high contrast",
  );
  check(
    html.includes('data-testid="remote-contrast"'),
    "the remote exposes a contrast control",
  );
  check(
    !html.includes('data-testid="remote-banner"'),
    "a healthy remote shows no reconnect banner",
  );
  check(
    !html.includes('data-testid="remote-wake-lock"'),
    "the server-rendered remote claims no wake lock",
  );

  const css = readFileSync(
    fileURLToPath(
      new URL("../src/features/remote/remote.css", import.meta.url),
    ),
    "utf8",
  );

  check(
    /\.remote button\s*\{[^}]*min-height:\s*44px/s.test(css),
    "every remote button has a 44 px minimum height",
  );
  check(
    /button\.remote__set-item\s*\{[^}]*min-height:\s*56px/s.test(css),
    "set-list rows are larger than the 44 px minimum",
  );
  check(
    /button\.remote__step-button\s*\{[^}]*min-height:\s*clamp\(56px/s.test(css),
    "previous/next buttons are at least 56 px high",
  );
  check(
    /button\.remote__panic-button\s*\{[^}]*min-height:\s*64px/s.test(css),
    "panic is a 64 px target",
  );
  check(
    /overflow-x:\s*clip/.test(css),
    "the remote clamps horizontal overflow",
  );
  check(
    /grid-template-columns:\s*minmax\(0, 1\.5fr\) minmax\(0, 1fr\)/.test(css),
    "the 1024 px layout uses two shrinkable columns",
  );
  check(
    /@media \(max-width: 820px\)/.test(css),
    "the remote is responsive below 820 px",
  );
  check(
    /(width|min-width):\s*(?:[5-9]\d|\d{3,})rem/.test(css) === false,
    "no fixed layout width exceeds the viewport",
  );

  const minWidths = [...css.matchAll(/min-width:\s*(\d+)px/g)].map((match) =>
    Number(match[1]),
  );

  check(
    minWidths.length > 0 &&
      minWidths.every((value) => value === 0 || value >= 44),
    "every pixel min-width is either zero or a 44 px+ touch target",
  );

  const loadingHtml = renderToStaticMarkup(
    <RemoteView
      model={remoteFromState(null)}
      connection="connecting"
      loading
    />,
  );

  check(
    loadingHtml.includes('aria-busy="true"'),
    "remote loading state is marked busy",
  );
  check(
    loadingHtml.includes("Loading the set"),
    "remote loading state names the set",
  );
}

export function remoteModelTest(): void {
  const model = remoteFromState(FIXTURE);

  check(model.patches.length === 8, "remote model keeps every set-list patch");
  check(model.armedSlot === 3, "remote model keeps the armed slot");
  check(stepPatch(model, 1) === 4, "next moves one patch forward in set order");
  check(
    stepPatch(model, -1) === 2,
    "previous moves one patch back in set order",
  );

  const last = { ...model, armedSlot: 7 };
  const first = { ...model, armedSlot: 0 };
  const none = { ...model, armedSlot: -1 };

  check(
    stepPatch(last, 1) === 0,
    "next wraps from the last patch to the first",
  );
  check(
    stepPatch(first, -1) === 7,
    "previous wraps from the first patch to the last",
  );
  check(
    stepPatch(none, 1) === 0,
    "next from a disarmed set starts at the first patch",
  );
  check(
    stepPatch(none, -1) === 7,
    "previous from a disarmed set starts at the last patch",
  );

  const sparse = {
    ...model,
    patches: [
      { id: "slot-1", name: "One", slot: 1, latencyMs: 0 },
      { id: "slot-5", name: "Five", slot: 5, latencyMs: 0 },
      { id: "slot-9", name: "Nine", slot: 9, latencyMs: 0 },
    ],
    armedSlot: 5,
  };

  check(
    stepPatch(sparse, 1) === 9 && stepPatch(sparse, -1) === 1,
    "next/previous follow set order across non-contiguous slot numbers",
  );
  check(
    stepPatch({ ...model, patches: [] }, 1) === null,
    "stepping an empty set produces no slot",
  );
  check(formatPosition(16.5) === "5.1", "position 16.5 PPQ is bar 5 beat 1");
  check(
    isRemotePath("/remote") &&
      isRemotePath("/remote/") &&
      !isRemotePath("/") &&
      !isRemotePath("/remote/extra"),
    "only the /remote path selects the remote route",
  );
}

export async function remoteInteractionTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    await store.refresh();

    const initial = remoteFromState(store.getSnapshot().state);
    const next = stepPatch(initial, 1);

    check(next === 4, "remote Next resolves to slot 4");
    check(
      await store.armSlot(next),
      "remote Next arms the patch through POST /arm",
    );

    const afterNext = renderToStaticMarkup(
      <RemoteView
        model={remoteFromState(store.getSnapshot().state)}
        connection="ready"
      />,
    );

    check(
      afterNext.includes(
        'data-testid="remote-set-item" data-slot="4" data-armed="true"',
      ),
      "the remote highlight follows Next",
    );
    check(
      afterNext.includes('data-testid="remote-current-name">Drum Kit<'),
      "the now-playing card follows Next",
    );

    check(
      await store.updateTransport({ playing: false }),
      "remote transport control reaches POST /transport",
    );
    check(
      server.transportRequests.length === 1 &&
        server.transportRequests[0]?.playing === false,
      "the engine received playing: false",
    );

    const paused = renderToStaticMarkup(
      <RemoteView
        model={remoteFromState(store.getSnapshot().state)}
        connection="ready"
      />,
    );

    check(
      paused.includes('data-testid="remote-transport-state">Stopped<'),
      "the remote renders the paused transport",
    );
    check(
      paused.includes('data-testid="remote-play"') && paused.includes(">Play<"),
      "the play button flips to Play",
    );

    check(await store.panic(), "panic stops the transport and disarms the set");
    check(
      store.getSnapshot().state?.armedSlot === -1,
      "panic leaves no armed slot",
    );
    check(
      server.armRequests.at(-1)?.slot === null,
      "panic sent a disarm request",
    );
    check(
      server.transportRequests.at(-1)?.playing === false,
      "panic stopped the transport",
    );
    check(
      server.transportRequests.at(-1)?.ppq === 0,
      "panic rewound the transport",
    );

    const panicked = renderToStaticMarkup(
      <RemoteView
        model={remoteFromState(store.getSnapshot().state)}
        connection="ready"
      />,
    );

    check(
      panicked.includes('data-testid="remote-current-name">Nothing armed<'),
      "the remote shows nothing armed after panic",
    );
    check(
      countOf(panicked, 'data-armed="true"') === 0,
      "no set-list row stays highlighted after panic",
    );
  } finally {
    await server.close();
  }
}

export function remoteBannerTest(): void {
  const live: RemoteLinkSnapshot = {
    connected: true,
    everConnected: true,
    lastLiveAt: 1,
    failures: 0,
  };

  const lost: RemoteLinkSnapshot = { ...live, connected: false, failures: 1 };

  check(!remoteBanner("ready", live).visible, "a live WS shows no banner");
  check(
    !remoteBanner("connecting", live).visible,
    "connecting does not show the banner",
  );

  const lostBanner = remoteBanner("ready", lost);

  check(
    lostBanner.visible && lostBanner.tone === "warning",
    "a lost WS shows the reconnecting banner",
  );
  check(
    lostBanner.title === "Live link lost",
    "the banner names the lost link",
  );
  check(
    lostBanner.detail.includes("Reconnecting automatically"),
    "the banner promises an automatic reconnect",
  );
  check(
    !remoteBanner("ready", { ...INITIAL_LINK }).visible,
    "no banner before the first connection",
  );

  const offline = remoteBanner("offline", live);

  check(
    offline.visible && offline.tone === "critical",
    "an offline control API shows the critical banner",
  );
  check(
    offline.title === "Engine unreachable",
    "the critical banner names the engine",
  );
}

class FakeWakeLockSentinel implements WakeLockSentinelLike {
  released = false;
  private readonly listeners: (() => void)[] = [];

  async release(): Promise<void> {
    this.released = true;

    for (const listener of this.listeners) listener();
  }

  addEventListener(_type: "release", listener: () => void): void {
    this.listeners.push(listener);
  }
}

class FakeWakeLockNavigator implements WakeLockCapableNavigator {
  readonly requests: string[] = [];
  readonly sentinels: FakeWakeLockSentinel[] = [];
  fail = false;

  readonly wakeLock = {
    request: async (type: "screen"): Promise<WakeLockSentinelLike> => {
      this.requests.push(type);

      if (this.fail) throw new Error("wake lock denied");

      const sentinel = new FakeWakeLockSentinel();
      this.sentinels.push(sentinel);

      return sentinel;
    },
  };
}

export async function remoteWakeLockTest(): Promise<void> {
  const none = new WakeLockController({ navigator: {} });

  check(
    (await none.start()) === "unavailable",
    "a browser without the wake-lock API reports unavailable",
  );

  const fake = new FakeWakeLockNavigator();
  const statuses: WakeLockStatus[] = [];
  const controller = new WakeLockController({
    navigator: fake,
    onStatus: (status) => {
      statuses.push(status);
    },
  });

  check(
    (await controller.start()) === "active",
    "the remote asks for a screen wake lock",
  );
  check(
    fake.requests.length === 1 && fake.requests[0] === "screen",
    "the request is for the screen",
  );
  check(controller.status === "active", "an acquired wake lock reports active");
  check(
    statuses.includes("requesting"),
    "the status passes through requesting",
  );

  check(
    (await controller.visibilityChanged(false)) === "idle",
    "hiding the page drops the lock",
  );
  check(
    fake.sentinels[0]?.released === true,
    "the hidden page's sentinel is released",
  );

  check(
    (await controller.visibilityChanged(true)) === "active",
    "returning to the page re-acquires the lock",
  );
  check(
    fake.requests.length === 2,
    "a second request is made after visibility returns",
  );

  await controller.release();

  check(
    controller.status === "idle",
    "releasing the controller returns to idle",
  );
  check(
    fake.sentinels.every((sentinel) => sentinel.released),
    "every sentinel is released",
  );

  const blocked = new FakeWakeLockNavigator();
  blocked.fail = true;

  const denied = new WakeLockController({ navigator: blocked });

  check(
    (await denied.start()) === "denied",
    "a rejected request reports denied",
  );
  check(denied.status === "denied", "the denied status is kept for the badge");

  await denied.release();
}

export function remoteContrastTest(): void {
  const storage = new MemoryStorage();

  check(
    DEFAULT_REMOTE_CONTRAST === "high",
    "the stage remote defaults to high contrast",
  );
  check(
    loadRemoteContrast(null) === "high",
    "contrast loads high without storage",
  );
  check(
    loadRemoteContrast(storage) === "high",
    "a fresh browser gets the high-contrast theme",
  );

  saveRemoteContrast(storage, "standard");
  check(
    loadRemoteContrast(storage) === "standard",
    "the contrast choice is persisted",
  );

  check(
    nextContrast("high") === "standard" && nextContrast("standard") === "high",
    "the contrast toggle flips both ways",
  );
  check(
    loadRemoteContrast({ getItem: () => "neon", setItem: () => {} }) === "high",
    "an unknown stored value falls back to high contrast",
  );
  check(
    loadRemoteContrast({
      getItem: () => {
        throw new Error("storage blocked");
      },
      setItem: () => {},
    }) === "high",
    "blocked storage falls back to high contrast",
  );

  const high = renderToStaticMarkup(
    <RemoteView
      model={remoteFromState(FIXTURE)}
      connection="ready"
      contrast="high"
      wakeLock="active"
    />,
  );

  check(high.includes('data-theme="stage"'), "remote declares the stage theme");
  check(
    high.includes('data-contrast="high"'),
    "remote renders the high-contrast mode",
  );
  check(
    high.includes(
      'data-testid="remote-contrast" data-contrast="high" aria-pressed="true"',
    ),
    "the contrast control reports high contrast",
  );
  check(high.includes(">High contrast<"), "the contrast control is labelled");
  check(
    high.includes('data-testid="remote-wake-lock" data-state="active"'),
    "the remote reports the screen wake lock",
  );
  check(high.includes(">Screen on<"), "the wake-lock badge explains itself");

  const standard = renderToStaticMarkup(
    <RemoteView
      model={remoteFromState(FIXTURE)}
      connection="ready"
      contrast="standard"
    />,
  );

  check(
    standard.includes('data-contrast="standard"') &&
      standard.includes('aria-pressed="false"'),
    "standard contrast renders when selected",
  );
  check(standard.includes(">Standard<"), "the standard mode is labelled");
  check(
    !standard.includes('data-testid="remote-wake-lock"'),
    "no wake-lock badge while the lock is idle",
  );

  const css = readFileSync(
    fileURLToPath(
      new URL("../src/features/remote/remote.css", import.meta.url),
    ),
    "utf8",
  );

  check(
    /\.remote\[data-contrast="high"\]\s*\{[^}]*--remote-bg:\s*#000000/s.test(
      css,
    ),
    "the high-contrast palette paints a pure black background",
  );
  check(
    /\.remote\[data-contrast="high"\]\s*\{[^}]*--remote-text:\s*#ffffff/s.test(
      css,
    ),
    "the high-contrast palette paints pure white text",
  );
  check(
    /\.remote\[data-contrast="high"\]\s+button\s*\{[^}]*border-color:\s*#ffffff/s.test(
      css,
    ),
    "high-contrast buttons get a white outline",
  );
  check(
    /\.remote__banner--warning/.test(css) &&
      /\.remote__banner--critical/.test(css),
    "both reconnect banner tones have styles",
  );
}

export async function remoteLinkTest(): Promise<void> {
  const sockets: FakeMeterSocket[] = [];
  const link = new MeterLink(
    new MeterStore({
      url: "ws://127.0.0.1:9/meters",
      socketFactory: () => {
        const socket = new FakeMeterSocket();
        sockets.push(socket);
        return socket;
      },
      reconnectBaseMs: 250,
      reconnectMaxMs: 250,
    }),
  );

  link.start();

  check(sockets.length === 1, "the remote opens one live WS");

  sockets[0]!.emit("open", {});
  check(link.getSnapshot().connected, "the live WS reports connected");
  check(
    link.getSnapshot().everConnected,
    "the remote remembers the link came up",
  );

  const liveHtml = renderToStaticMarkup(
    <RemoteView
      model={remoteFromState(FIXTURE)}
      connection="ready"
      link={link.getSnapshot()}
    />,
  );

  check(
    !liveHtml.includes('data-testid="remote-banner"'),
    "no banner while the WS is up",
  );

  const killedAt = Date.now();
  sockets[0]!.close();

  check(!link.getSnapshot().connected, "killing the WS marks the link down");
  check(link.getSnapshot().failures === 1, "the drop is counted");

  const downHtml = renderToStaticMarkup(
    <RemoteView
      model={remoteFromState(FIXTURE)}
      connection="ready"
      link={link.getSnapshot()}
    />,
  );

  check(
    downHtml.includes('data-testid="remote-banner"'),
    "killing the WS shows the reconnect banner",
  );
  check(
    downHtml.includes('data-tone="warning"'),
    "the banner warns instead of blocking the set list",
  );
  check(downHtml.includes("Live link lost"), "the banner names the lost link");

  await waitFor(() => sockets.length >= 2, 3000);

  sockets[1]!.emit("open", {});
  const recoveredMs = Date.now() - killedAt;

  check(
    recoveredMs < 3000,
    `the WS reconnects within 3 s (${String(recoveredMs)} ms)`,
  );
  check(link.getSnapshot().connected, "the reconnected WS reports connected");

  const recoveredHtml = renderToStaticMarkup(
    <RemoteView
      model={remoteFromState(FIXTURE)}
      connection="ready"
      link={link.getSnapshot()}
    />,
  );

  check(
    !recoveredHtml.includes('data-testid="remote-banner"'),
    "the banner clears after the automatic reconnect",
  );

  link.stop();

  check(
    !link.getSnapshot().connected && !link.getSnapshot().everConnected,
    "stopping the link resets its state",
  );
}

export async function remoteOptimisticTest(): Promise<void> {
  const server = await startFixtureServer({ armDelayMs: 250 });

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    await store.refresh();

    const before = renderToStaticMarkup(
      <RemoteView
        model={remoteFromState(store.getSnapshot().state)}
        connection="ready"
      />,
    );

    check(
      before.includes(
        'data-testid="remote-set-item" data-slot="3" data-armed="true"',
      ),
      "the remote starts on the armed slot",
    );

    const startedAt = Date.now();
    const pendingArm = store.armSlot(5);

    const optimistic = renderToStaticMarkup(
      <RemoteView
        model={remoteFromState(store.getSnapshot().state)}
        connection="ready"
      />,
    );

    check(
      optimistic.includes(
        'data-testid="remote-set-item" data-slot="5" data-armed="true"',
      ),
      "the remote highlight moves before POST /arm answers",
    );
    check(
      optimistic.includes('data-testid="remote-current-name">Analog Brass<'),
      "the now-playing card follows the tap optimistically",
    );
    check(
      !optimistic.includes('data-slot="3" data-armed="true"'),
      "the old highlight is gone immediately",
    );
    check(
      Date.now() - startedAt < 200,
      "the highlight moves before the delayed arm response",
    );

    check(await pendingArm, "POST /arm confirms the optimistic arm");
    check(
      server.armRequests.length === 1 && server.armRequests[0]?.slot === 5,
      "the engine received the tapped slot",
    );

    const pendingTransport = store.updateTransport({ playing: false });

    const paused = renderToStaticMarkup(
      <RemoteView
        model={remoteFromState(store.getSnapshot().state)}
        connection="ready"
      />,
    );

    check(
      paused.includes('data-testid="remote-transport-state">Stopped<'),
      "Play/Pause flips before POST /transport answers",
    );
    check(
      paused.includes('data-testid="remote-play"') && paused.includes(">Play<"),
      "the play button flips optimistically",
    );

    check(
      await pendingTransport,
      "POST /transport confirms the optimistic pause",
    );
    check(
      server.transportRequests.at(-1)?.playing === false,
      "the engine received playing: false",
    );
  } finally {
    await server.close();
  }
}
