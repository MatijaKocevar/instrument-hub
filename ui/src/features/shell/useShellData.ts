import { useEffect, useMemo } from "react";

import {
    gridFromState,
    metersUrlFor,
    useMeters,
} from "../grid";
import {
    instrumentSlotForRoute,
    patchForInstrumentSlot,
} from "../instrument";
import { pluginAssignmentsFromState } from "../plugins";
import { useAppState, useAppStore } from "../../state/AppStoreContext";
import {
    slotFromSearch,
    type RouterState,
} from "../../state/router";

/**
    Subscribe to the store, derive the current page model and load the data
    each route needs on arrival.
*/
export function useShellData (router: RouterState)
{
    const app = useAppState ();
    const store = useAppStore ();
    const state = app.state;
    const grid = useMemo (() => gridFromState (state), [state]);
    const pluginAssignments = useMemo (() => pluginAssignmentsFromState (state), [state]);
    const meters = useMeters ({ url: metersUrlFor (store.baseUrl) });

    const armedSlot = state?.armedSlot ?? -1;
    const requestedSlot = slotFromSearch (router.search);
    const instrumentSlot = instrumentSlotForRoute (router.route, requestedSlot, armedSlot);
    const instrumentPatch = patchForInstrumentSlot (
        state?.families.flatMap (family => family.patches) ?? [],
        instrumentSlot);

    // The insert chain belongs to the Instrument page, and the inventory feeds
    // both pages. Fetch each on arrival; the store de-duplicates the chain per
    // slot and keeps a successful inventory until a rescan replaces it.
    useEffect (() =>
    {
        if (router.route !== "instrument" && router.route !== "plugins")
            return;

        if (router.route === "instrument" && instrumentSlot !== null)
            void store.loadFxChain (instrumentSlot);

        if (app.plugins === null && ! app.pluginBusy)
            void store.loadPlugins ();
    }, [router.route, instrumentSlot, store, app.plugins, app.pluginBusy]);

    // The audio page also shows the MIDI inputs. The normal poll carries them,
    // but a direct route render (and the snapshot test) may arrive first.
    useEffect (() =>
    {
        if (router.route !== "audio" || app.midi !== null || app.midiBusy)
            return;

        void store.loadMidi ();
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
