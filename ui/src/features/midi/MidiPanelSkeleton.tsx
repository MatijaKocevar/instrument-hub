import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./midi.css";

function MidiRowSkeleton(): ReactElement {
  return (
    <li className="midi__input midi__input--skeleton" aria-hidden="true">
      <div className="midi__input-label">
        <Skeleton width="1.1rem" height="1.1rem" radius="0.25rem" />
        <div className="midi__skeleton-main">
          <Skeleton width="48%" height="0.9rem" />
          <div className="midi__skeleton-meta">
            <Skeleton width="4rem" height="0.8rem" radius="999px" />
            <Skeleton width="3rem" height="0.8rem" radius="999px" />
          </div>
        </div>
      </div>
      <Skeleton width="5rem" height="1rem" radius="999px" />
    </li>
  );
}

export function MidiPanelSkeleton(): ReactElement {
  return (
    <section
      className="midi"
      aria-label="MIDI inputs"
      aria-busy="true"
      data-testid="midi-panel"
      data-loading="true"
    >
      <header className="midi__head">
        <div>
          <p className="midi__eyebrow">Controllers</p>
          <h2 className="midi__title">MIDI</h2>
        </div>
        <Skeleton width="9rem" height="2rem" radius="999px" />
      </header>

      <ul className="midi__inputs" aria-hidden="true">
        {[0, 1, 2].map((index) => (
          <MidiRowSkeleton key={index} />
        ))}
      </ul>

      <p className="loading-sr-only" role="status" data-testid="midi-loading">
        Reading the MIDI inputs…
      </p>
    </section>
  );
}
