import type { Client } from "openapi-fetch";

import type { RequestGuard } from "./client";
import type { paths } from "./schema";
import type { ControlProjectCommand, ControlProjectOverview } from "./types";

export interface ProjectEndpoints {
  getProject(options?: {
    signal?: AbortSignal;
  }): Promise<ControlProjectOverview>;

  updateProject(
    command: ControlProjectCommand,
    options?: { signal?: AbortSignal },
  ): Promise<ControlProjectOverview>;
}

function requireProject(
  guard: RequestGuard,
  data: ControlProjectOverview | undefined,
  error: unknown,
  status: number,
  what: string,
): ControlProjectOverview {
  if (
    data !== undefined &&
    typeof data.path === "string" &&
    typeof data.name === "string" &&
    typeof data.dirty === "boolean" &&
    typeof data.saved === "boolean" &&
    Array.isArray(data.recent)
  )
    return data;

  return guard.fail(error, status, what);
}

export function createProjectEndpoints(
  client: Client<paths>,
  guard: RequestGuard,
): ProjectEndpoints {
  return {
    async getProject(requestOptions) {
      const { data, error, response } = await client.GET("/project", {
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireProject(
        guard,
        data,
        error,
        response.status,
        "GET /project",
      );
    },

    async updateProject(command, requestOptions) {
      const { data, error, response } = await client.POST("/project", {
        body: command,
        ...(requestOptions?.signal !== undefined
          ? { signal: requestOptions.signal }
          : {}),
      });

      return requireProject(
        guard,
        data,
        error,
        response.status,
        "POST /project",
      );
    },
  };
}
