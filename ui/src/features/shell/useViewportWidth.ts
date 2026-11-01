import { useEffect, useState } from "react";

export const NARROW_BREAKPOINT = 900;

export function useViewportWidth(initialWidth?: number | undefined): number {
  const [width, setWidth] = useState<number>(
    () =>
      initialWidth ??
      (typeof window === "undefined" ? 1280 : window.innerWidth),
  );

  useEffect(() => {
    if (typeof window === "undefined") return undefined;

    const onResize = (): void => {
      setWidth(window.innerWidth);
    };

    window.addEventListener("resize", onResize);

    return () => {
      window.removeEventListener("resize", onResize);
    };
  }, []);

  return width;
}
