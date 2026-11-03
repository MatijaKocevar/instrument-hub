import type { ReactElement } from "react";

import { GridView } from "../grid";
import type { RouterState } from "../../state/router";
import { armAndOpenInstrument } from "./armAndOpenInstrument";
import type { ShellData } from "./useShellData";

export function GridRoute({
  router,
  shell,
}: {
  readonly router: RouterState;
  readonly shell: ShellData;
}): ReactElement {
  const { state, grid, meters, store } = shell;

  const openPatch = (slot: number): void => {
    armAndOpenInstrument(
      slot,
      (candidate) => {
        void store.armSlot(candidate);
      },
      router.navigate,
    );
  };

  const changeColour = (slot: number, colorIndex: number): void => {
    void store.updatePatch(slot, { colorIndex });
  };

  return (
    <GridView
      model={grid}
      meters={meters}
      loading={state === null}
      onArm={openPatch}
      onColourChange={changeColour}
      onFamilyColourChange={(familyId, _slots, colorIndex) => {
        void store.updateFamilyColour(familyId, colorIndex);
      }}
    />
  );
}
