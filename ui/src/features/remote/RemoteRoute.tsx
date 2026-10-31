import { useMemo, type ReactElement } from "react";

import { useAppState, useAppStore } from "../../state/AppStoreContext";
import { RemoteView } from "./RemoteView";
import { remoteFromState, stepPatch } from "./model";

/**
    Store-connected `/remote` route.

    Arming and transport changes go through `AppStore`, so the remote gets the
    same optimistic updates and rollback behaviour as the desktop grid.
*/
export function RemoteRoute (): ReactElement
{
    const app = useAppState ();
    const store = useAppStore ();
    const model = useMemo (() => remoteFromState (app.state), [app.state]);

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
