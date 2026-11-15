export type ToastSeverity = "error" | "warning" | "success" | "info";

export interface ToastInput {
  readonly severity: ToastSeverity;
  readonly title: string;
  readonly detail?: string;
  readonly meta?: string;
  readonly key?: string;
  readonly sticky?: boolean;
}

export interface Toast {
  readonly id: number;
  readonly key: string;
  readonly severity: ToastSeverity;
  readonly title: string;
  readonly detail: string;
  readonly meta: string;
  readonly sticky: boolean;
  readonly repeat: number;
  readonly updatedAt: number;
}
