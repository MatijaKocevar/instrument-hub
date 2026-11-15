import { useEffect } from "react";

import type { Toast, ToastSeverity } from "../../state/toastTypes";

const DURATION_MS: Record<ToastSeverity, number> = {
  error: 10_000,
  warning: 8_000,
  success: 4_000,
  info: 5_000,
};

export function useAutoDismiss(
  toast: Toast,
  paused: boolean,
  onDismiss: (id: number) => void,
): void {
  useEffect(() => {
    if (toast.sticky || paused) return undefined;

    const timer = setTimeout(() => {
      onDismiss(toast.id);
    }, DURATION_MS[toast.severity]);

    return () => {
      clearTimeout(timer);
    };
  }, [
    onDismiss,
    paused,
    toast.id,
    toast.severity,
    toast.sticky,
    toast.updatedAt,
  ]);
}
