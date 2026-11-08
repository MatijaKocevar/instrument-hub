import {
  useCallback,
  useEffect,
  useRef,
  useState,
  type KeyboardEvent as ReactKeyboardEvent,
  type PointerEvent as ReactPointerEvent,
} from "react";

import {
  TRACK_MAX_GAIN,
  trackDbForGain,
  trackGainForDb,
  trackGainForPointerRatio,
} from "./trackVolume";

const emitIntervalMs = 50;

interface TrackVolumeInputOptions {
  readonly gain: number;
  readonly disabled: boolean;
  readonly onChange: ((gain: number) => void) | undefined;
}

export interface TrackVolumeInput {
  readonly dragging: boolean;
  readonly onPointerDown: (event: ReactPointerEvent<HTMLElement>) => void;
  readonly onPointerMove: (event: ReactPointerEvent<HTMLElement>) => void;
  readonly onPointerUp: (event: ReactPointerEvent<HTMLElement>) => void;
  readonly onPointerCancel: (event: ReactPointerEvent<HTMLElement>) => void;
  readonly onLostPointerCapture: () => void;
  readonly onKeyDown: (event: ReactKeyboardEvent<HTMLElement>) => void;
}

export function useTrackVolumeInput({
  gain,
  disabled,
  onChange,
}: TrackVolumeInputOptions): TrackVolumeInput {
  const [dragging, setDragging] = useState(false);
  const draggingRef = useRef(false);
  const latest = useRef(gain);
  const lastSent = useRef(gain);
  const lastSentAt = useRef(0);
  const pending = useRef<number | null>(null);
  const timer = useRef<number | null>(null);
  const onChangeRef = useRef(onChange);
  onChangeRef.current = onChange;

  if (!draggingRef.current) latest.current = gain;

  const clearTimer = useCallback(() => {
    if (timer.current === null) return;

    window.clearTimeout(timer.current);
    timer.current = null;
  }, []);

  useEffect(() => clearTimer, [clearTimer]);

  const send = useCallback((value: number) => {
    latest.current = value;
    lastSent.current = value;
    lastSentAt.current = performance.now();
    onChangeRef.current?.(value);
  }, []);

  const queue = useCallback(
    (value: number) => {
      latest.current = value;

      if (onChangeRef.current === undefined) return;

      const now = performance.now();
      const elapsed = now - lastSentAt.current;

      if (elapsed >= emitIntervalMs) {
        clearTimer();
        pending.current = null;
        send(value);
        return;
      }

      pending.current = value;

      if (timer.current !== null) return;

      timer.current = window.setTimeout(() => {
        timer.current = null;
        const next = pending.current;
        pending.current = null;

        if (next !== null) send(next);
      }, emitIntervalMs - elapsed);
    },
    [clearTimer, send],
  );

  const commit = useCallback(() => {
    clearTimer();
    pending.current = null;

    if (latest.current !== lastSent.current) send(latest.current);
  }, [clearTimer, send]);

  const gainAt = (event: ReactPointerEvent<HTMLElement>): number => {
    const rect = event.currentTarget.getBoundingClientRect();

    if (rect.height <= 0) return TRACK_MAX_GAIN;

    return trackGainForPointerRatio(
      1 - (event.clientY - rect.top) / rect.height,
    );
  };

  const finish = (): void => {
    if (!draggingRef.current) return;

    draggingRef.current = false;
    setDragging(false);
    commit();
  };

  const onPointerDown = (event: ReactPointerEvent<HTMLElement>): void => {
    if (disabled || draggingRef.current || event.button !== 0) return;

    if (
      event.target instanceof Element &&
      event.target.closest("button") !== null
    )
      return;

    event.preventDefault();
    event.currentTarget.focus();
    event.currentTarget.setPointerCapture(event.pointerId);
    draggingRef.current = true;
    setDragging(true);
    queue(gainAt(event));
  };

  const onPointerMove = (event: ReactPointerEvent<HTMLElement>): void => {
    if (!draggingRef.current) return;

    queue(gainAt(event));
  };

  const onPointerUp = (event: ReactPointerEvent<HTMLElement>): void => {
    if (!draggingRef.current) return;

    queue(gainAt(event));
    finish();

    if (event.currentTarget.hasPointerCapture(event.pointerId))
      event.currentTarget.releasePointerCapture(event.pointerId);
  };

  const onPointerCancel = (event: ReactPointerEvent<HTMLElement>): void => {
    finish();

    if (event.currentTarget.hasPointerCapture(event.pointerId))
      event.currentTarget.releasePointerCapture(event.pointerId);
  };

  const onKeyDown = (event: ReactKeyboardEvent<HTMLElement>): void => {
    if (
      disabled ||
      event.target !== event.currentTarget ||
      onChangeRef.current === undefined
    )
      return;

    const currentDb = trackDbForGain(latest.current);
    let next: number;

    switch (event.key) {
      case "ArrowUp":
      case "ArrowRight":
        next = trackGainForDb(currentDb + 0.5);
        break;
      case "ArrowDown":
      case "ArrowLeft":
        next = trackGainForDb(currentDb - 0.5);
        break;
      case "PageUp":
        next = trackGainForDb(currentDb + 3);
        break;
      case "PageDown":
        next = trackGainForDb(currentDb - 3);
        break;
      case "Home":
        next = 0;
        break;
      case "End":
        next = TRACK_MAX_GAIN;
        break;
      default:
        return;
    }

    event.preventDefault();
    send(next);
  };

  return {
    dragging,
    onPointerDown,
    onPointerMove,
    onPointerUp,
    onPointerCancel,
    onLostPointerCapture: finish,
    onKeyDown,
  };
}
