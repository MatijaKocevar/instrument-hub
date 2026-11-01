import { useEffect, useMemo } from "react";

import { gridFromState, metersUrlFor, useMeters } from "../grid";
import { instrumentSlotForRoute, patchForInstrumentSlot } from "../instrument";
import { pluginAssignmentsFromState } from "../plugins";
import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { slotFromSearch, type RouterState } from "../../state/router";

export function useShellData(router: RouterState) {
  const app = useAppState();
  const store = useAppStore();
  const state = app.state;
  const grid = useMemo(() => gridFromState(state), [state]);
  const pluginAssignments = useMemo(
    () => pluginAssignmentsFromState(state),
    [state],
  );
  const meters = useMeters({ url: metersUrlFor(store.baseUrl) });

  const armedSlot = state?.armedSlot ?? -1;
  const requestedSlot = slotFromSearch(router.search);
  const instrumentSlot = instrumentSlotForRoute(
    router.route,
    requestedSlot,
    armedSlot,
  );
  const instrumentPatch = patchForInstrumentSlot(
    state?.families.flatMap((family) => family.patches) ?? [],
    instrumentSlot,
  );

  useEffect(() => {
    if (router.route !== "instrument" && router.route !== "plugins") return;

    if (router.route === "instrument" && instrumentSlot !== null)
      void store.loadFxChain(instrumentSlot);

    if (app.plugins === null && !app.pluginBusy) void store.loadPlugins();
  }, [router.route, instrumentSlot, store, app.plugins, app.pluginBusy]);

  useEffect(() => {
    if (router.route !== "audio" || app.midi !== null || app.midiBusy) return;

    void store.loadMidi();
  }, [router.route, store, app.midi, app.midiBusy]);

  return {
    app,
    store,
    state,
    grid,
    pluginAssignments,
    meters,
    armedSlot,
    instrumentSlot,
    instrumentPatch,
    version: state?.application.version ?? "0.1.0",
  };
}

export type ShellData = ReturnType<typeof useShellData>;
