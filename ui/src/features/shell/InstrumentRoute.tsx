import type { ReactElement } from "react";

import {
    followsArmedSlot,
    InstrumentPage,
} from "../instrument";
import type { ShellData } from "./useShellData";

export function InstrumentRoute ({ shell }: {
    readonly shell: ShellData;
}): ReactElement
{
    const { app, store, state, armedSlot, instrumentSlot, instrumentPatch } = shell;

    const changeColour = (slot: number, colorIndex: number): void =>
    {
        void store.updatePatch (slot, { colorIndex });
    };

    return (
        <InstrumentPage
            patch={instrumentPatch}
            armedSlot={armedSlot}
            following={followsArmedSlot (armedSlot)}
            chain={app.fxSlot === instrumentSlot ? app.fxChain : null}
            chainError={app.fxSlot === instrumentSlot ? app.fxError : null}
            chainBusy={app.fxBusy}
            sourceBusy={app.sourceBusy}
            editorOpen={instrumentPatch?.editorOpen === true}
            editorBusy={app.editorBusy}
            editorError={app.editorError}
            captureBusy={app.captureBusy}
            capture={app.captureSlot === instrumentSlot ? app.capture : null}
            captureError={app.captureSlot === instrumentSlot ? app.captureError : null}
            plugins={app.plugins?.plugins ?? null}
            pluginError={app.pluginError}
            loading={state === null}
            onColourChange={changeColour}
            onVolumeChange={(slot, volumeDb) => {
                void store.updatePatch (slot, { volumeDb });
            }}
            onPanChange={(slot, pan) => {
                void store.updatePatch (slot, { pan });
            }}
            onMuteChange={(slot, mute) => {
                void store.updatePatch (slot, { mute });
            }}
            onSoloChange={(slot, solo) => {
                void store.updatePatch (slot, { solo });
            }}
            onSourceChange={(slot, source) => {
                void store.changeInstrumentSource (slot, source);
            }}
            onOpenEditor={slot => { void store.openInstrumentEditor (slot); }}
            onCloseEditor={slot => { void store.closeInstrumentEditor (slot); }}
            onCaptureState={slot => { void store.savePatchSound (slot); }}
            onAddInsert={(slot, insert) => {
                void store.addFxInsert (slot, insert);
            }}
            onRemoveInsert={(slot, index) => {
                void store.removeFxInsert (slot, index);
            }}
            onToggleBypass={(slot, index, bypassed) => {
                void store.updateFxInsert (slot, index, { bypassed });
            }}
            onMoveInsert={(slot, from, to) => {
                void store.moveFxInsert (slot, from, to);
            }}
            onRetryChain={slot => { void store.loadFxChain (slot, { force: true }); }}
        />
    );
}
