import type { ReactElement } from "react";

import { GridView, metersUrlFor } from "../grid";
import type { RouterState } from "../../state/router";
import type { ShellData } from "./useShellData";

export function GridRoute({
  router,
  shell,
}: {
  readonly router: RouterState;
  readonly shell: ShellData;
}): ReactElement {
  const { state, grid, store } = shell;

  const selectTrack = (index: number): void => {
    void store.selectTrack(index);
  };

  const openTrack = (index: number): void => {
    void store.selectTrack(index);
    router.navigate("/instrument");
  };

  return (
    <GridView
      model={grid}
      metersUrl={metersUrlFor(store.baseUrl, store.token)}
      loading={state === null}
      onSelectTrack={selectTrack}
      onOpenTrack={openTrack}
      onToggleTrackFx={(index, hot) => {
        void store.toggleTrackFx(index, hot);
      }}
      onSetTrackVolume={(index, gain) => {
        void store.setTrackVolume(index, gain);
      }}
      onRenameTrack={(index, name) => {
        void store.renameTrack(index, name);
      }}
    />
  );
}
