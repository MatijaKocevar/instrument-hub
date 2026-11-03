import { readJsonObject, sendJson } from "../http";
import type { FixtureRoute } from "../types";

function projectName(path: string): string {
  const file = path.split("/").pop() ?? "";
  return file.replace(/\.ihub$/i, "");
}

function remember(
  context: Parameters<FixtureRoute>[0],
  path: string,
  name: string,
): void {
  context.project.path = path;
  context.project.name = name;
  context.project.recent = [
    { path, name },
    ...context.project.recent.filter((entry) => entry.path !== path),
  ];
  context.state.project = {
    path: context.project.path,
    name: context.project.name,
    dirty: context.project.dirty,
    saved: context.project.saved,
    error: context.project.error,
  };
}

export const handleProject: FixtureRoute = (context, request, response) => {
  if (request.url !== "/project") return false;

  if (request.method === "GET") {
    sendJson(response, 200, context.project);
    return true;
  }

  if (request.method !== "POST") return false;

  readJsonObject(request, response, (body) => {
    const action = body.action;

    context.projectRequests.push({
      action: typeof action === "string" ? action : "",
      path: typeof body.path === "string" ? body.path : "",
    });

    if (
      action !== "new" &&
      action !== "open" &&
      action !== "save" &&
      action !== "save-as" &&
      action !== "close"
    ) {
      sendJson(response, 400, { error: "unknown project action" });
      return;
    }

    const path = typeof body.path === "string" ? body.path : "";

    if ((action === "open" || action === "save-as") && path.length === 0) {
      sendJson(response, 400, { error: `${action} needs a project path` });
      return;
    }

    if (action === "new") {
      const target = path.length > 0 ? path : "/tmp/Untitled.ihub";
      const name =
        typeof body.name === "string" && body.name.length > 0
          ? body.name
          : projectName(target);

      context.project.dirty = false;
      context.project.saved = true;
      context.project.error = "";
      remember(context, target, name);
    } else if (action === "open") {
      context.project.dirty = false;
      context.project.saved = true;
      context.project.error = "";
      remember(context, path, projectName(path));
    } else if (action === "save") {
      context.project.dirty = false;
      context.project.saved = true;
      context.project.error = "";
      remember(context, context.project.path, context.project.name);
    } else if (action === "save-as") {
      context.project.dirty = false;
      context.project.saved = true;
      context.project.error = "";
      remember(context, path, projectName(path));
    } else {
      context.project.path = "";
      context.project.name = "";
      context.project.dirty = false;
      context.project.saved = false;
      context.project.error = "";
      context.state.project = {
        path: "",
        name: "",
        dirty: false,
        saved: false,
        error: "",
      };
    }

    sendJson(response, 200, context.project);
  });

  return true;
};
