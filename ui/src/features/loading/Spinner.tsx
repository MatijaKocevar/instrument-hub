import type { ReactElement } from "react";

import "./loading.css";

export interface SpinnerProps {
  readonly label?: string;
  readonly className?: string | undefined;
}

export function Spinner({
  label = "Working",
  className,
}: SpinnerProps): ReactElement {
  return (
    <span
      className={
        className === undefined
          ? "loading-spinner"
          : `loading-spinner ${className}`
      }
      role="status"
      aria-label={label}
      data-testid="spinner"
    >
      <span className="loading-spinner__ring" aria-hidden="true" />
    </span>
  );
}
