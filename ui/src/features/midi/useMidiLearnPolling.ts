import { useEffect, useRef } from "react";

export function useMidiLearnPolling(
  active: boolean,
  refresh?: (() => void) | undefined,
): void {
  const callback = useRef(refresh);
  callback.current = refresh;

  useEffect(() => {
    if (!active) return;

    const timer = setInterval(() => {
      callback.current?.();
    }, 250);

    return () => {
      clearInterval(timer);
    };
  }, [active]);
}
