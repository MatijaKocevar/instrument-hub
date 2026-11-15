import type { Toast, ToastInput } from "./toastTypes";

const MAX_VISIBLE = 5;

function identity(input: ToastInput): string {
  return (
    input.key ??
    `${input.severity}\u0000${input.title}\u0000${input.detail ?? ""}`
  );
}

export class ToastCenter {
  private toasts: readonly Toast[] = [];
  private nextId = 1;
  private readonly listeners = new Set<() => void>();

  getSnapshot = (): readonly Toast[] => this.toasts;

  subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);

    return () => {
      this.listeners.delete(listener);
    };
  };

  notify = (input: ToastInput): void => {
    const key = identity(input);
    const existing = this.toasts.find((toast) => toast.key === key);

    if (existing !== undefined) {
      const detail = input.detail ?? "";
      const repeat =
        existing.severity === input.severity &&
        existing.title === input.title &&
        existing.detail === detail
          ? existing.repeat + 1
          : 1;

      this.commit(
        this.toasts.map((toast) =>
          toast.key === key
            ? {
                ...toast,
                severity: input.severity,
                title: input.title,
                detail,
                meta: input.meta ?? "",
                sticky: input.sticky ?? false,
                repeat,
                updatedAt: Date.now(),
              }
            : toast,
        ),
      );

      return;
    }

    const toast: Toast = {
      id: this.nextId,
      key,
      severity: input.severity,
      title: input.title,
      detail: input.detail ?? "",
      meta: input.meta ?? "",
      sticky: input.sticky ?? false,
      repeat: 1,
      updatedAt: Date.now(),
    };

    this.nextId += 1;
    this.commit([...this.toasts, toast]);
  };

  resolve = (key: string, input: ToastInput): void => {
    if (!this.toasts.some((toast) => toast.key === key)) return;

    this.commit(
      this.toasts.map((toast) =>
        toast.key === key
          ? {
              ...toast,
              severity: input.severity,
              title: input.title,
              detail: input.detail ?? "",
              meta: input.meta ?? "",
              sticky: input.sticky ?? false,
              updatedAt: Date.now(),
            }
          : toast,
      ),
    );
  };

  dismiss = (id: number): void => {
    if (!this.toasts.some((toast) => toast.id === id)) return;

    this.commit(this.toasts.filter((toast) => toast.id !== id));
  };

  dismissKey = (key: string): void => {
    if (!this.toasts.some((toast) => toast.key === key)) return;

    this.commit(this.toasts.filter((toast) => toast.key !== key));
  };

  clear = (): void => {
    if (this.toasts.length === 0) return;

    this.commit([]);
  };

  private commit(next: readonly Toast[]): void {
    this.toasts = this.cap(next);

    for (const listener of this.listeners) listener();
  }

  private cap(next: readonly Toast[]): readonly Toast[] {
    if (next.length <= MAX_VISIBLE) return next;

    let overflow = next.length - MAX_VISIBLE;
    const kept = next.filter((toast) => {
      if (overflow === 0 || toast.sticky) return true;

      overflow -= 1;
      return false;
    });

    return kept.length <= MAX_VISIBLE
      ? kept
      : kept.slice(kept.length - MAX_VISIBLE);
  }
}
