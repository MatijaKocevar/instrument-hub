import type { ReactElement } from "react";

import { metersUrlFor } from "../grid";
import { followsArmedSlot, InstrumentPage } from "../instrument";
import type { RouterState } from "../../state/router";
import type { ShellData } from "./useShellData";

export function InstrumentRoute({
  router,
  shell,
}: {
  readonly router: RouterState;
  readonly shell: ShellData;
}): ReactElement {
  const {
    app,
    store,
    state,
    armedSlot,
    instrumentSlot,
    instrumentPatch,
    grid,
    track,
  } = shell;
  const gridTrack =
    grid.tracks.find((candidate) => candidate.slot === instrumentSlot) ?? null;

  return (
    <InstrumentPage
      patch={instrumentPatch}
      armedSlot={armedSlot}
      following={followsArmedSlot(armedSlot)}
      chain={app.fxSlot === instrumentSlot ? app.fxChain : null}
      chainUnavailable={app.fxSlot === instrumentSlot && app.fxError !== null}
      chainBusy={app.fxBusy}
      sourceBusy={app.sourceBusy}
      editorOpen={instrumentPatch?.editorOpen === true}
      editorBusy={app.editorBusy}
      reloadBusy={app.reloadBusy && app.reloadSlot === instrumentSlot}
      captureBusy={app.captureBusy}
      plugins={app.plugins?.plugins ?? null}
      pluginsUnavailable={app.pluginError !== null}
      audioInputs={app.audio?.inputChannels ?? null}
      track={track}
      gridTrack={gridTrack}
      metersUrl={metersUrlFor(store.baseUrl, store.token)}
      loading={state === null}
      onPanChange={(slot, pan) => {
        void store.updatePatch(slot, { pan });
      }}
      onMuteChange={(slot, mute) => {
        void store.updatePatch(slot, { mute });
      }}
      onSoloChange={(slot, solo) => {
        void store.updatePatch(slot, { solo });
      }}
      onSourceChange={(slot, source) => {
        void store.changeInstrumentSource(slot, source);
      }}
      onOpenEditor={(slot) => {
        void store.openInstrumentEditor(slot);
      }}
      onReloadPlugin={(slot) => {
        void store.reloadInstrumentPlugin(slot);
      }}
      onCaptureState={(slot) => {
        void store.savePatchSound(slot);
      }}
      onAddInsert={(slot, insert) => {
        void store.addFxInsert(slot, insert);
      }}
      onRemoveInsert={(slot, index) => {
        void store.removeFxInsert(slot, index);
      }}
      onToggleBypass={(slot, index, bypassed) => {
        void store.updateFxInsert(slot, index, { bypassed });
      }}
      onMoveInsert={(slot, from, to) => {
        void store.moveFxInsert(slot, from, to);
      }}
      onUpdateInsert={(slot, index, update) => {
        void store.updateFxInsert(slot, index, update);
      }}
      onReadTuner={(slot, index) => store.getTuner(slot, index)}
      onRetryChain={(slot) => {
        void store.loadFxChain(slot, { force: true });
      }}
      onAssignHotFx={(trackIndex, hot, insertId) => {
        void store.assignTrackHotFx(trackIndex, hot, insertId);
      }}
      onRenameTrack={(trackIndex, name) => {
        void store.renameTrack(trackIndex, name);
      }}
      onSelectTrack={(trackIndex) => {
        void store.selectTrack(trackIndex);
      }}
      onOpenTrack={(trackIndex) => {
        void store.selectTrack(trackIndex);
        router.navigate("/instrument");
      }}
      onToggleTrackFx={(trackIndex, hot) => {
        void store.toggleTrackFx(trackIndex, hot);
      }}
      onSetTrackVolume={(trackIndex, gain) => {
        void store.setTrackVolume(trackIndex, gain);
      }}
    />
  );
}
