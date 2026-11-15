import { useState, type ReactElement } from "react";

import type { Toast } from "../../state/toastTypes";
import { ToastIcon } from "./ToastIcon";
import { useAutoDismiss } from "./useAutoDismiss";

export interface ToastCardProps {
  readonly toast: Toast;
  readonly onDismiss: (id: number) => void;
}

export function ToastCard({ toast, onDismiss }: ToastCardProps): ReactElement {
  const [expanded, setExpanded] = useState(false);
  const [hovered, setHovered] = useState(false);
  const [focused, setFocused] = useState(false);
  const expandable = toast.detail.length > 0;
  const role =
    toast.severity === "error" || toast.severity === "warning"
      ? "alert"
      : "status";
  const time = new Date(toast.updatedAt).toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
  });

  useAutoDismiss(toast, expanded || hovered || focused, onDismiss);

  const summary = (
    <>
      <ToastIcon severity={toast.severity} />

      <span className="toast__copy">
        <span className="toast__headline">
          <span className="toast__title">{toast.title}</span>
          {toast.repeat > 1 ? (
            <span className="toast__repeat">×{toast.repeat}</span>
          ) : null}
        </span>

        {expandable ? (
          <span className="toast__preview" title={toast.detail}>
            {toast.detail}
          </span>
        ) : null}
      </span>

      {expandable ? (
        <span className="toast__chevron" aria-hidden="true">
          ›
        </span>
      ) : null}
    </>
  );

  return (
    <article
      className={`toast toast--${toast.severity}`}
      role={role}
      data-testid="toast"
      data-severity={toast.severity}
      data-key={toast.key}
      data-expanded={expanded ? "true" : "false"}
      data-sticky={toast.sticky ? "true" : "false"}
      onMouseEnter={() => {
        setHovered(true);
      }}
      onMouseLeave={() => {
        setHovered(false);
      }}
      onFocus={() => {
        setFocused(true);
      }}
      onBlur={() => {
        setFocused(false);
      }}
      onKeyDown={(event) => {
        if (event.key === "Escape") onDismiss(toast.id);
      }}
    >
      {expandable ? (
        <button
          className="toast__summary"
          type="button"
          aria-expanded={expanded}
          data-testid="toast-toggle"
          onClick={() => {
            setExpanded((current) => !current);
          }}
        >
          {summary}
        </button>
      ) : (
        <div className="toast__summary">{summary}</div>
      )}

      <button
        className="toast__dismiss"
        type="button"
        aria-label={`Dismiss notification: ${toast.title}`}
        data-testid="toast-dismiss"
        onClick={() => {
          onDismiss(toast.id);
        }}
      >
        ×
      </button>

      {expanded ? (
        <div className="toast__body" data-testid="toast-body">
          <p className="toast__detail">{toast.detail}</p>
          <p className="toast__meta">
            {toast.meta.length > 0 ? <span>{toast.meta}</span> : null}
            <span>{time}</span>
            {toast.sticky ? <span>Stays until resolved</span> : null}
          </p>
        </div>
      ) : null}
    </article>
  );
}
