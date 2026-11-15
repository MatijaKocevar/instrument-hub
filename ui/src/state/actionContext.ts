import type { ControlApi } from "../api/client";
import type { AppSnapshot } from "./snapshot";
import type { ToastInput } from "./toastTypes";

export interface ActionContext {
  readonly api: ControlApi;
  readonly now: () => number;
  readonly getSnapshot: () => AppSnapshot;
  readonly patch: (change: Partial<AppSnapshot>) => void;
  readonly notify: (input: ToastInput) => void;
}
