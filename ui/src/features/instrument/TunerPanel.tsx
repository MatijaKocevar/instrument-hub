import { useEffect, useRef, useState, type ReactElement } from "react";

import type { ControlFxTunerState } from "../../api/client";
import { TunerGauge } from "./TunerGauge";

interface TunerPanelProps {
  readonly slot: number;
  readonly index: number;
  readonly active: boolean;
  readonly read?: (slot: number, index: number) => Promise<ControlFxTunerState>;
}

const pollIntervalMs = 60;

export function TunerPanel({
  slot,
  index,
  active,
  read,
}: TunerPanelProps): ReactElement {
  const [reading, setReading] = useState<ControlFxTunerState | null>(null);
  const readRef = useRef(read);
  readRef.current = read;

  useEffect(() => {
    const reader = readRef.current;

    if (!active || reader === undefined) {
      setReading(null);
      return;
    }

    let cancelled = false;
    let inFlight = false;

    const readOnce = async (): Promise<void> => {
      if (inFlight || cancelled) return;

      inFlight = true;

      try {
        const next = await reader(slot, index);

        if (!cancelled) setReading(next);
      } catch {
        if (!cancelled) setReading(null);
      } finally {
        inFlight = false;
      }
    };

    void readOnce();
    const timer = window.setInterval(() => {
      void readOnce();
    }, pollIntervalMs);

    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [active, slot, index]);

  return (
    <div
      className="instrument__tuner"
      data-testid="fx-tuner"
      data-active={active ? "true" : "false"}
    >
      <TunerGauge reading={reading} active={active} />
      <p className="instrument__tuner-hint" data-testid="fx-tuner-hint">
        {active
          ? "A4 440 Hz · place first for the cleanest reading"
          : "Arm this track to tune"}
      </p>
    </div>
  );
}
