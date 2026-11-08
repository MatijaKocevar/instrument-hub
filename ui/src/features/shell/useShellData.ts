import { useCallback, useEffect, useMemo, useRef } from "react";

import { gridFromState } from "../grid";
import {
  instrumentSlotForRoute,
  patchForInstrumentSlot,
  patchForTrack,
  trackForSlot,
} from "../instrument";
import { pluginAssignmentsFromState } from "../plugins";
import { useAppState, useAppStore } from "../../state/AppStoreContext";
import {
  slotFromSearch,
  type RouterState,
  type ShellRoute,
} from "../../state/router";

export function useShellData(router: RouterState) {
  const app = useAppState();
  const store = useAppStore();
  const state = app.state;
  const grid = useMemo(() => gridFromState(state), [state]);
  const pluginAssignments = useMemo(
    () => pluginAssignmentsFromState(state),
    [state],
  );
  const armedSlot = state?.armedSlot ?? -1;
  const requestedSlot = slotFromSearch(router.search);
  const instrumentSlot = instrumentSlotForRoute(
    router.route,
    requestedSlot,
    armedSlot,
  );
  const track =
    state?.layout === "tracks"
      ? trackForSlot(state.tracks, instrumentSlot)
      : null;
  const instrumentPatch =
    state !== null && track !== null
      ? patchForTrack(state, track)
      : patchForInstrumentSlot(
          state?.families.flatMap((family) => family.patches) ?? [],
          instrumentSlot,
        );
  const prefetched = useRef(false);
  const prefetchInstrumentSlot = armedSlot >= 0 ? armedSlot : requestedSlot;

  const prefetchRoute = useCallback(
    (route: ShellRoute): void => {
      if (
        (route === "plugins" || route === "instrument") &&
        app.plugins === null &&
        !app.pluginBusy
      )
        void store.loadPlugins();

      if (route === "audio" && app.midi === null && !app.midiBusy)
        void store.loadMidi();

      if (route === "instrument" && prefetchInstrumentSlot !== null)
        void store.loadFxChain(prefetchInstrumentSlot);
    },
    [
      app.midi,
      app.midiBusy,
      app.pluginBusy,
      app.plugins,
      prefetchInstrumentSlot,
      store,
    ],
  );

  useEffect(() => {
    store.clearInstrumentError();
  }, [instrumentSlot, store]);

  useEffect(() => {
    if (state === null || prefetched.current) return;

    prefetched.current = true;

    if (app.plugins === null && !app.pluginBusy) void store.loadPlugins();
    if (app.midi === null && !app.midiBusy) void store.loadMidi();
  }, [app.midi, app.midiBusy, app.pluginBusy, app.plugins, state, store]);

  useEffect(() => {
    if (router.route !== "instrument" && router.route !== "plugins") return;

    if (router.route === "instrument" && instrumentSlot !== null)
      void store.loadFxChain(instrumentSlot);

    const snapshot = store.getSnapshot();

    if (snapshot.plugins === null && !snapshot.pluginBusy)
      void store.loadPlugins();
  }, [router.route, instrumentSlot, store]);

  useEffect(() => {
    if (router.route !== "audio") return;

    const snapshot = store.getSnapshot();

    if (snapshot.midi === null && !snapshot.midiBusy) void store.loadMidi();
  }, [router.route, store]);

  return {
    app,
    store,
    state,
    live: app.live,
    grid,
    pluginAssignments,
    armedSlot,
    instrumentSlot,
    instrumentPatch,
    track,
    prefetchRoute,
    version: state?.application.version ?? "0.1.0",
  };
}

export type ShellData = ReturnType<typeof useShellData>;
