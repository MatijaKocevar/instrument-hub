import {
  createControlApi,
  type ControlAudioMeasurement,
  type ControlAudioState,
  type ControlAudioWizard,
  type ControlState,
  type ControlTransport,
} from "../../api/client";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { check, waitFor } from "../harness";

export async function reconnectTest(): Promise<void> {
  const server = await startFixtureServer({ failFirst: 2 });

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      retryBaseMs: 5,
      retryMaxMs: 20,
      pollIntervalMs: 60_000,
    });

    store.start();

    try {
      await waitFor(() => store.getSnapshot().phase === "ready", 3000);
    } finally {
      store.stop();
    }

    const snapshot = store.getSnapshot();

    check(snapshot.phase === "ready", "store reconnects after HTTP failures");
    check(
      snapshot.state?.armedSlot === 3,
      "reconnected store holds the fetched state",
    );
    check(
      snapshot.failedAttempts === 0,
      "failure counter resets after a success",
    );
    check(server.stateRequests >= 3, "reconnect retried the request");
  } finally {
    await server.close();
  }
}

export async function backoffTest(): Promise<void> {
  const failingApi = {
    baseUrl: "http://127.0.0.1:1",
    token: null as string | null,
    setToken(): void {},
    getState: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    armSlot: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    selectTrack: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    toggleTrackFx: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    assignTrackFx: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    setTrackVolume: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    updatePatch: async (): Promise<ControlState> => {
      throw new Error("connection refused");
    },
    controlTransport: async (): Promise<ControlTransport> => {
      throw new Error("connection refused");
    },
    getAudio: async (): Promise<ControlAudioState> => {
      throw new Error("connection refused");
    },
    updateAudio: async (): Promise<ControlAudioState> => {
      throw new Error("connection refused");
    },
    measureLatency: async (): Promise<ControlAudioMeasurement> => {
      throw new Error("connection refused");
    },
    recommendAudioSetup: async (): Promise<ControlAudioWizard> => {
      throw new Error("connection refused");
    },
    applyAudioSetup: async (): Promise<ControlAudioState> => {
      throw new Error("connection refused");
    },
    getPairing: async () => {
      throw new Error("connection refused");
    },
    setLanMode: async () => {
      throw new Error("connection refused");
    },
    pair: async () => {
      throw new Error("connection refused");
    },
    revokePairing: async () => {
      throw new Error("connection refused");
    },
    changePatchSource: async () => {
      throw new Error("connection refused");
    },
    openPatchEditor: async () => {
      throw new Error("connection refused");
    },
    closePatchEditor: async () => {
      throw new Error("connection refused");
    },
    capturePatchState: async () => {
      throw new Error("connection refused");
    },
    getFxChain: async () => {
      throw new Error("connection refused");
    },
    addFxInsert: async () => {
      throw new Error("connection refused");
    },
    replaceFxChain: async () => {
      throw new Error("connection refused");
    },
    updateFxInsert: async () => {
      throw new Error("connection refused");
    },
    removeFxInsert: async () => {
      throw new Error("connection refused");
    },
    reorderFxChain: async () => {
      throw new Error("connection refused");
    },
    listPlugins: async () => {
      throw new Error("connection refused");
    },
    rescanPlugins: async () => {
      throw new Error("connection refused");
    },
    getMidi: async () => {
      throw new Error("connection refused");
    },
    setMidiInput: async () => {
      throw new Error("connection refused");
    },
    runMidiPadTest: async () => {
      throw new Error("connection refused");
    },
    getProject: async () => {
      throw new Error("connection refused");
    },
    updateProject: async () => {
      throw new Error("connection refused");
    },
  };

  const store = new AppStore(failingApi, { retryBaseMs: 5, retryMaxMs: 15 });

  await store.refresh();
  check(store.retryDelayMs() === 5, "first retry waits for the base delay");

  await store.refresh();
  check(store.retryDelayMs() === 10, "second retry doubles the delay");

  await store.refresh();
  check(store.retryDelayMs() === 15, "retry delay is capped");

  check(
    store.getSnapshot().phase === "offline",
    "store reports offline after failures",
  );
  check(
    store.getSnapshot().error === "connection refused",
    "store keeps the failure message",
  );
}
