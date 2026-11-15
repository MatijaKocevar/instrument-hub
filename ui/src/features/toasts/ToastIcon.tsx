import type { ReactElement } from "react";

import type { ToastSeverity } from "../../state/toastTypes";

export interface ToastIconProps {
  readonly severity: ToastSeverity;
}

export function ToastIcon({ severity }: ToastIconProps): ReactElement {
  return (
    <svg
      className="toast__icon"
      viewBox="0 0 16 16"
      width="18"
      height="18"
      aria-hidden="true"
      focusable="false"
    >
      {severity === "success" ? (
        <path
          d="M3.4 8.4 6.5 11.5 12.6 4.6"
          fill="none"
          stroke="currentColor"
          strokeLinecap="round"
          strokeLinejoin="round"
          strokeWidth="1.8"
        />
      ) : severity === "warning" ? (
        <>
          <path
            d="M8 2.3 14.2 13H1.8L8 2.3Z"
            fill="none"
            stroke="currentColor"
            strokeLinejoin="round"
            strokeWidth="1.4"
          />
          <path
            d="M8 6v3.5"
            stroke="currentColor"
            strokeLinecap="round"
            strokeWidth="1.6"
          />
          <circle cx="8" cy="11.6" r="0.9" fill="currentColor" />
        </>
      ) : severity === "info" ? (
        <>
          <circle
            cx="8"
            cy="8"
            r="6.1"
            fill="none"
            stroke="currentColor"
            strokeWidth="1.4"
          />
          <path
            d="M8 7.2v4"
            stroke="currentColor"
            strokeLinecap="round"
            strokeWidth="1.6"
          />
          <circle cx="8" cy="4.9" r="0.9" fill="currentColor" />
        </>
      ) : (
        <>
          <circle
            cx="8"
            cy="8"
            r="6.1"
            fill="none"
            stroke="currentColor"
            strokeWidth="1.4"
          />
          <path
            d="M8 4.8v4"
            stroke="currentColor"
            strokeLinecap="round"
            strokeWidth="1.6"
          />
          <circle cx="8" cy="11.1" r="0.9" fill="currentColor" />
        </>
      )}
    </svg>
  );
}
