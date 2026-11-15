import { useSyncExternalStore, type ReactElement } from "react";

import { useAppStore } from "../../state/AppStoreContext";
import { ToastCard } from "./ToastCard";
import "./toast.css";

export interface ToastViewportProps {
  readonly surface?: "desktop" | "remote";
}

export function ToastViewport({
  surface = "desktop",
}: ToastViewportProps): ReactElement {
  const store = useAppStore();
  const toasts = useSyncExternalStore(
    store.subscribeToasts,
    store.getToasts,
    store.getToasts,
  );

  return (
    <section
      className="toast-stack"
      data-surface={surface}
      data-count={toasts.length}
      data-testid="toast-viewport"
      aria-label="Notifications"
    >
      {toasts.length > 1 ? (
        <button
          className="toast-stack__clear"
          type="button"
          data-testid="toast-clear-all"
          onClick={store.clearToasts}
        >
          Clear all
        </button>
      ) : null}

      <div className="toast-stack__list">
        {toasts.map((toast) => (
          <ToastCard
            key={toast.id}
            toast={toast}
            onDismiss={store.dismissToast}
          />
        ))}
      </div>
    </section>
  );
}
