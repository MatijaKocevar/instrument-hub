import {
  useContext,
  useEffect,
  useRef,
  useState,
  type CSSProperties,
  type ReactElement,
} from "react";

import { meterLevelForPeak } from "./meterLevel";
import { TrackMetersContext } from "./TrackMetersContext";
import "./trackMeter.css";

const PEAK_HOLD_MS = 1200;

export interface TrackLevelMeterProps {
  readonly slot: number;
  readonly index: number;
  readonly active: boolean;
}

function peakText(live: boolean, peak: number, db: number): string {
  if (!live || peak <= 0) return "−∞ dB";

  return `${db.toFixed(1)} dB`;
}

export function TrackLevelMeter({
  slot,
  index,
  active,
}: TrackLevelMeterProps): ReactElement {
  const meters = useContext(TrackMetersContext);
  const live = active && meters.connected;
  const peak = live ? (meters.slots[slot]?.peak ?? 0) : 0;
  const level = meterLevelForPeak(peak);
  const [hold, setHold] = useState(0);
  const lastPeakAt = useRef(0);
  const [clipped, setClipped] = useState(false);

  useEffect(() => {
    if (!live) {
      if (hold !== 0) setHold(0);
      lastPeakAt.current = 0;
      return;
    }

    const now = Date.now();

    if (level.percent >= hold) {
      lastPeakAt.current = now;

      if (level.percent > hold) setHold(level.percent);

      return;
    }

    if (now - lastPeakAt.current >= PEAK_HOLD_MS && level.percent !== hold)
      setHold(level.percent);
  });

  useEffect(() => {
    if (!live || peak < 1) return;

    setClipped(true);
  }, [live, peak]);

  useEffect(() => {
    if (!live) setClipped(false);
  }, [live]);

  const percent = level.percent.toFixed(1);
  const showClip = live && clipped;
  const fillStyle = {
    clipPath: `inset(${(100 - level.percent).toFixed(1)}% 0 0 0)`,
  } satisfies CSSProperties;
  const holdStyle = {
    bottom: `${hold.toFixed(1)}%`,
  } satisfies CSSProperties;

  return (
    <div
      className="track-grid__meter-layer"
      data-testid="track-meter"
      data-slot={slot}
      data-connected={live ? "true" : "false"}
    >
      <span
        className="track-grid__fader-rail"
        data-testid="track-fader-rail"
        aria-hidden="true"
      >
        <span
          className="track-grid__meter-fill"
          data-testid="track-meter-fill"
          data-percent={percent}
          style={fillStyle}
        />
        <span
          className="track-grid__meter-hold"
          data-testid="track-meter-hold"
          data-percent={hold.toFixed(1)}
          data-visible={live && hold > 0 ? "true" : "false"}
          style={holdStyle}
        />
      </span>

      <span
        className="track-grid__fader-peak"
        data-testid="track-fader-peak"
        data-db={live && peak > 0 ? level.db.toFixed(1) : "-inf"}
        data-live={live ? "true" : "false"}
        aria-hidden="true"
      >
        {peakText(live, peak, level.db)}
      </span>

      {showClip ? (
        <button
          type="button"
          className="track-grid__meter-clip"
          data-testid="track-meter-clip"
          aria-label={`Clear track ${String(index + 1)} clip`}
          onClick={() => {
            setClipped(false);
          }}
        >
          CLIP
        </button>
      ) : null}
    </div>
  );
}
