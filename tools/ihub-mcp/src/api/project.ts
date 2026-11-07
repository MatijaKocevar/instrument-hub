import type { Client } from "openapi-fetch";
import type { components, paths } from "./schema.js";
import { unwrap } from "./result.js";

type ProjectCommand = components["schemas"]["ProjectCommand"];
type ProjectOverview = components["schemas"]["ProjectOverview"];
type HubClient = Client<paths>;

export interface ProjectApi {
  updateProject(command: ProjectCommand): Promise<ProjectOverview>;
}

export function createProjectApi(client: HubClient): ProjectApi {
  return {
    updateProject: async (command) =>
      unwrap(await client.POST("/project", { body: command })),
  };
}
