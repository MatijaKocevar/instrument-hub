import { createControlApi } from "../../api/client";
import { AppStore } from "../../state/store";
import { FIXTURE } from "../fixtures";
import { FakeStateSocket } from "../fakes/stateSocket";
import { startFixtureServer } from "../fixtureServer";
import { check, countOf, renderApp, waitFor } from "../harness";

function frameFor(
  revision: number,
  armedSlot: number,
  kind: string,
  sequence = 10,
): unknown {
  const state = {
    ...FIXTURE,
    revision,
    armedSlot,
    slots: FIXTURE.slots.map((slot) => ({
      ...slot,
      armed: slot.index === armedSlot,
    })),
  };

  return {
    type: "state",
    sequence,
    revision,
    timestampMs: 100,
    event: { kind, pad: -1, slot: armedSlot, pressed: false },
    pads: [],
    state,
  };
}

export async function statePushTest(): Promise<void> {
  const server = await startFixtureServer();
  const socket = new FakeStateSocket();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
      stateSocketFactory: () => socket,
    });

    store.start();

    try {
      await waitFor(() => store.getSnapshot().phase === "ready", 3000);

      const baseRequests = server.stateRequests;

      socket.emit("open", {});
      check(store.getSnapshot().live, "state socket reports connected");

      socket.push(frameFor(2, 7, "arm"));

      await waitFor(() => store.getSnapshot().state?.armedSlot === 7, 1000);

      check(
        server.stateRequests === baseRequests,
        "the pushed state is applied without another GET /state",
      );

      const html = renderApp(store, "/grid");
      check(
        html.includes('data-slot="7" data-armed="true"'),
        "the pushed arm moves the grid highlight",
      );
      check(
        countOf(html, 'data-armed="true"') === 1,
        "the pushed state keeps exactly one armed cell",
      );

      socket.push(frameFor(1, 3, "arm", 11));
      check(
        store.getSnapshot().state?.armedSlot === 7,
        "a stale frame cannot roll the store back",
      );

      socket.close();
      check(!store.getSnapshot().live, "socket abort clears the live flag");
    } finally {
      store.stop();
    }
  } finally {
    await server.close();
  }
}

export async function pollFallbackTest(): Promise<void> {
  const server = await startFixtureServer();
  const sockets: FakeStateSocket[] = [];

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 80,
      retryBaseMs: 10,
      retryMaxMs: 20,
      stateSocketFactory: () => {
        const socket = new FakeStateSocket();
        sockets.push(socket);
        return socket;
      },
    });

    store.start();

    try {
      await waitFor(() => store.getSnapshot().phase === "ready", 3000);
      sockets[0]?.close();

      const startedAt = Date.now();
      const response = await fetch(`${server.baseUrl}/arm`, {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ slot: 6 }),
      });

      check(response.status === 200, "the external POST /arm succeeded");

      await waitFor(() => store.getSnapshot().state?.armedSlot === 6, 3000);

      check(
        Date.now() - startedAt < 3000,
        "polling applies the external arm within three seconds",
      );
      check(
        store.getSnapshot().state?.armedSlot === 6,
        "the fallback poll moved the store to slot 6",
      );
      check(server.stateRequests > 1, "the fallback poll fetched state again");
    } finally {
      store.stop();
    }
  } finally {
    await server.close();
  }
}
