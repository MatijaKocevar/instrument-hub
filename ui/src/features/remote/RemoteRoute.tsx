import { useEffect, useMemo, useRef, type ReactElement } from "react";

import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { codeFromSearch, PairingGate } from "../pairing";
import { RemoteView } from "./RemoteView";
import { remoteFromState, stepPatch } from "./model";

/**
    Store-connected `/remote` route.

    Arming and transport changes go through `AppStore`, so the remote gets the
    same optimistic updates and rollback behaviour as the desktop grid.

    A tablet that has not paired yet sees the pairing gate instead of an empty
    stage. The QR link carries `?code=...`, which is redeemed automatically.
*/
export function RemoteRoute (): ReactElement
{
    const app = useAppState ();
    const store = useAppStore ();
    const model = useMemo (() => remoteFromState (app.state), [app.state]);
    const initialCode = useMemo (
        () => (typeof window === "undefined" ? null : codeFromSearch (window.location.search)),
        []);
    const attempted = useRef<string | null> (null);

    useEffect (() =>
    {
        if (initialCode === null || store.token !== null || attempted.current === initialCode)
            return;

        attempted.current = initialCode;
        void store.pair (initialCode);
    }, [initialCode, store]);

    const step = (direction: 1 | -1): void =>
    {
        const slot = stepPatch (model, direction);

        if (slot !== null)
            void store.armSlot (slot);
    };

    if (app.unauthorized)
    {
        return (
            <PairingGate
                pairing={app.pairing}
                busy={app.pairingBusy}
                error={app.pairingError ?? app.error}
                initialCode={initialCode}
                onPair={code => { void store.pair (code); }}
            />
        );
    }

    return (
        <RemoteView
            model={model}
            connection={app.phase}
            loading={app.state === null}
            error={app.error}
            onSelect={slot => { void store.armSlot (slot); }}
            onPrevious={() => { step (-1); }}
            onNext={() => { step (1); }}
            onTogglePlay={() => {
                void store.updateTransport ({ playing: ! model.transport.playing });
            }}
            onStop={() => {
                void store.updateTransport ({ playing: false, ppq: 0, positionSamples: 0 });
            }}
            onPanic={() => { void store.panic (); }}
        />
    );
}
