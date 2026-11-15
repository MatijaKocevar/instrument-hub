import { useEffect, useRef } from "react";

import type { ControlProjectOverview } from "../../api/client";
import { useAppStore } from "../../state/AppStoreContext";

const KEY = "project-state";

interface ProjectError {
  readonly path: string;
  readonly error: string;
}

export function useProjectStatusToasts(
  project: ControlProjectOverview | null,
): void {
  const store = useAppStore();
  const previous = useRef<ProjectError | null>(null);

  useEffect(() => {
    if (project === null) {
      previous.current = null;
      return;
    }

    const prior = previous.current;

    if (prior !== null && prior.path !== project.path)
      store.dismissToastKey(KEY);

    previous.current = { path: project.path, error: project.error };

    if (project.error.length > 0) {
      if (
        prior === null ||
        prior.error !== project.error ||
        prior.path !== project.path
      ) {
        store.notify({
          severity: "error",
          title: "Project error",
          detail: project.error,
          key: KEY,
          sticky: true,
        });
      }
      return;
    }

    if (
      prior !== null &&
      prior.path === project.path &&
      prior.error.length > 0
    ) {
      store.resolveToast(KEY, {
        severity: "success",
        title: "Project saved",
        key: KEY,
      });
    }
  }, [project, store]);
}
