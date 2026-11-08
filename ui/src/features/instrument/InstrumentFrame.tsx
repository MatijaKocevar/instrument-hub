import type { ReactElement, ReactNode } from "react";

import type { ControlPatch, ControlTrack } from "../../api/client";
import { InstrumentHead } from "./InstrumentHead";
import { InstrumentSkeleton } from "./InstrumentSkeleton";

interface InstrumentFrameProps {
  readonly patch: ControlPatch | null;
  readonly armedSlot: number;
  readonly following: boolean;
  readonly loading: boolean;
  readonly track: ControlTrack | null;
  readonly onRenameTrack?: (index: number, name: string) => void;
  readonly children?: ReactNode;
}

export function InstrumentFrame({
  patch,
  armedSlot,
  following,
  loading,
  track,
  onRenameTrack,
  children,
}: InstrumentFrameProps): ReactElement {
  if (patch === null && loading)
    return <InstrumentSkeleton following={following} />;

  if (patch === null) {
    return (
      <section
        className="instrument instrument--empty"
        data-testid="instrument-page"
        data-slot={-1}
        data-following={following ? "true" : "false"}
        aria-busy="false"
      >
        <header className="instrument__head">
          <p className="instrument__eyebrow">Instrument</p>
          <h2 className="instrument__title" data-testid="instrument-name">
            No patch selected
          </h2>
          <p className="instrument__lede">
            Arm a patch on the Grid page, or press a pad on the Launchpad, to
            open its instrument.
          </p>
        </header>
      </section>
    );
  }

  const armed = patch.slot === armedSlot;

  return (
    <section
      className="instrument"
      data-testid="instrument-page"
      data-slot={patch.slot}
      data-armed={armed ? "true" : "false"}
      data-following={following ? "true" : "false"}
      data-warm-state={patch.warmState}
      data-colour-index={patch.colorIndex}
    >
      <InstrumentHead
        patch={patch}
        armed={armed}
        following={following}
        track={track}
        {...(onRenameTrack !== undefined ? { onRenameTrack } : {})}
      />

      {children}
    </section>
  );
}
