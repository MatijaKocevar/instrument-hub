import { useEffect, useMemo, useRef, useState, type ReactElement } from "react";

import { metersUrlFor } from "../grid";
import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { codeFromSearch, PairingGate } from "../pairing";
import { RemoteView } from "./RemoteView";
import { useRemoteLink } from "./link";
import { remoteFromState, stepPatch } from "./model";
import {
    loadRemoteContrast,
    nextContrast,
    saveRemoteContrast,
    type ContrastStorage,
    type RemoteContrast,
} from "./theme";
import { useWakeLock } from "./wakeLock";

function contrastStorage (): ContrastStorage | null
{
    return typeof localStorage === "undefined" ? null : localStorage;
}

/**
    Store-connected `/remote` route.

    Arming and transport changes go through `AppStore`, so the remote gets the
    same optimistic updates and rollback behaviour as the desktop grid. A
    dropped live socket is visible as a banner and reconnects on its own.

    A tablet that has not paired yet sees the pairing gate instead of an empty
    stage. The QR link carries `?code=...`, which is redeemed automatically.
*/
export function RemoteRoute (): ReactElement
{
    const app = useAppState ();
    const store = useAppStore ();
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

    return <RemoteStage />;
}

/**
    The authorised stage.

    Kept separate from the gate so the live socket and the wake lock only run
    once a token exists; an unpaired tablet must not open a socket it will be
    rejected from every 250 ms.
*/
function RemoteStage (): ReactElement
{
    const app = useAppState ();
    const store = useAppStore ();
    const model = useMemo (() => remoteFromState (app.state), [app.state]);

    const metersUrl = useMemo (() => metersUrlFor (store.baseUrl), [store.baseUrl]);
    const link = useRemoteLink (metersUrl);
    const wakeLock = useWakeLock ();

    const [contrast, setContrast] = useState<RemoteContrast> (
        () => loadRemoteContrast (contrastStorage ()));

    // A tablet that was asleep should not wait for the next poll, and the
    // browser needs a fresh wake-lock request when it becomes visible again.
    useEffect (() =>
    {
        const onVisibilityChange = (): void =>
        {
            if (document.visibilityState === "visible")
                void store.refresh ();
        };

        document.addEventListener ("visibilitychange", onVisibilityChange);

        return () =>
        {
            document.removeEventListener ("visibilitychange", onVisibilityChange);
        };
    }, [store]);

    const toggleContrast = (): void =>
    {
        setContrast (current =>
        {
            const next = nextContrast (current);
            saveRemoteContrast (contrastStorage (), next);

            return next;
        });
    };

    const step = (direction: 1 | -1): void =>
    {
        const slot = stepPatch (model, direction);

        if (slot !== null)
            void store.armSlot (slot);
    };

    return (
        <RemoteView
            model={model}
            connection={app.phase}
            loading={app.state === null}
            error={app.error}
            link={link}
            contrast={contrast}
            wakeLock={wakeLock}
            onToggleContrast={toggleContrast}
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
