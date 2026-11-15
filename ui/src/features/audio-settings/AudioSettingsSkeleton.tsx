import type { ReactElement } from "react";

import { Skeleton } from "../loading";
import "./audio-settings.css";

function DeviceCardSkeleton(): ReactElement {
  return (
    <div className="audio-settings__device-card audio-settings__device-card--skeleton">
      <Skeleton width="64%" height="0.95rem" />
      <Skeleton width="42%" height="0.75rem" />
      <Skeleton width="3.75rem" height="1rem" radius="999px" />
    </div>
  );
}

function ChannelMapSkeleton({ title }: { title: string }): ReactElement {
  return (
    <fieldset className="audio-settings__channels">
      <legend>{title}</legend>
      <div className="audio-settings__channel-list">
        {[0, 1, 2].map((index) => (
          <div className="audio-settings__skeleton-channel" key={index}>
            <Skeleton width="1.1rem" height="1.1rem" radius="0.25rem" />
            <Skeleton width="40%" height="0.8rem" />
            <Skeleton width="1.5rem" height="0.7rem" />
          </div>
        ))}
      </div>
    </fieldset>
  );
}

export function AudioSettingsSkeleton(): ReactElement {
  return (
    <section
      className="audio-settings"
      aria-label="Audio settings"
      aria-busy="true"
      data-testid="audio-settings"
    >
      <header className="audio-settings__header">
        <div>
          <p className="audio-settings__eyebrow">Engine</p>
          <h2 className="audio-settings__title">Audio settings</h2>
        </div>
        <Skeleton width="8.5rem" height="2rem" radius="999px" />
      </header>

      <section
        className="audio-settings__picker"
        aria-label="Output device"
        aria-hidden="true"
      >
        <header className="audio-settings__picker-head">
          <div>
            <p className="audio-settings__eyebrow">Output</p>
            <h3 className="audio-settings__picker-title">Device</h3>
          </div>
          <Skeleton width="5.5rem" height="2.5rem" radius="10px" />
        </header>

        <div className="audio-settings__device-grid">
          {[0, 1, 2].map((index) => (
            <DeviceCardSkeleton key={index} />
          ))}
        </div>
      </section>

      <div className="audio-settings__controls" aria-hidden="true">
        {[0, 1, 2].map((index) => (
          <div className="audio-settings__skeleton-control" key={index}>
            <Skeleton width="45%" height="0.7rem" />
            <Skeleton width="100%" height="2.75rem" radius="10px" />
          </div>
        ))}
      </div>

      <div className="audio-settings__maps" aria-hidden="true">
        <ChannelMapSkeleton title="Input map" />
        <ChannelMapSkeleton title="Output map" />
      </div>

      <div className="audio-settings__readout" aria-hidden="true">
        <div className="audio-settings__skeleton-readout">
          <Skeleton width="5rem" height="0.7rem" />
          <Skeleton width="4rem" height="1.1rem" />
        </div>
        <div className="audio-settings__skeleton-readout">
          <Skeleton width="3rem" height="0.7rem" />
          <Skeleton width="2.5rem" height="1.7rem" />
        </div>
        <div className="audio-settings__skeleton-readout">
          <Skeleton width="6rem" height="0.75rem" />
          <Skeleton width="100%" height="2.75rem" radius="10px" />
        </div>
      </div>

      <p
        className="loading-sr-only"
        role="status"
        data-testid="audio-settings-loading"
      >
        Waiting for the audio device…
      </p>
    </section>
  );
}
