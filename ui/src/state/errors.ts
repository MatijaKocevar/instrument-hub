import { ApiError } from "../api/client";
import type { ToastInput } from "./toastTypes";

export function messageOf(error: unknown): string {
  if (error instanceof Error && error.message.length > 0) return error.message;

  if (typeof error === "string" && error.length > 0) return error;

  return "the control API is unreachable";
}

export function failureToast(
  title: string,
  error: unknown,
  key?: string,
): ToastInput {
  const status = error instanceof ApiError ? error.status : 0;

  return {
    severity: "error",
    title,
    detail: messageOf(error),
    ...(status > 0 ? { meta: `HTTP ${String(status)}` } : {}),
    ...(key === undefined ? {} : { key }),
  };
}
