import type { CSSProperties, ReactElement } from "react";

import "./loading.css";

export interface SkeletonProps {
  readonly className?: string | undefined;
  readonly width?: string | number | undefined;
  readonly height?: string | number | undefined;
  readonly radius?: string | number | undefined;
}

export function Skeleton({
  className,
  width,
  height,
  radius,
}: SkeletonProps): ReactElement {
  const style: CSSProperties = {};

  if (width !== undefined) style.width = width;
  if (height !== undefined) style.height = height;
  if (radius !== undefined) style.borderRadius = radius;

  return (
    <span
      className={className === undefined ? "skeleton" : `skeleton ${className}`}
      style={style}
      aria-hidden="true"
      data-testid="skeleton"
    />
  );
}
