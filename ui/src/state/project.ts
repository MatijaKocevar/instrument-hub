import type { ControlProjectCommand } from "../api/client";
import type { ActionContext } from "./actionContext";
import { failureToast } from "./errors";

export class ProjectActions {
  constructor(private readonly context: ActionContext) {}

  loadProject = async (): Promise<boolean> => {
    try {
      const project = await this.context.api.getProject();

      this.patch({ project });
      return true;
    } catch {
      return false;
    }
  };

  newProject = (path?: string, name?: string): Promise<boolean> =>
    this.run(
      {
        action: "new",
        ...(path !== undefined ? { path } : {}),
        ...(name !== undefined ? { name } : {}),
      },
      true,
      "Could not create project",
      "Project created",
    );

  openProject = (path: string): Promise<boolean> =>
    this.run(
      { action: "open", path },
      true,
      "Could not open project",
      "Project opened",
    );

  openProjectDialog = async (): Promise<boolean> => {
    const path = await this.chooseProjectPath("open");

    return path === null ? false : this.openProject(path);
  };

  saveProject = (): Promise<boolean> => {
    const project = this.context.getSnapshot().project;

    return project === null || project.path.length === 0
      ? this.saveProjectAsDialog()
      : this.run(
          { action: "save" },
          false,
          "Could not save project",
          "Project saved",
        );
  };

  saveProjectAs = (path: string): Promise<boolean> =>
    this.run(
      { action: "save-as", path },
      false,
      "Could not save project",
      "Project saved",
    );

  saveProjectAsDialog = async (): Promise<boolean> => {
    const path = await this.chooseProjectPath("save");

    return path === null ? false : this.saveProjectAs(path);
  };

  closeProject = (): Promise<boolean> =>
    this.run(
      { action: "close" },
      false,
      "Could not close project",
      "Project closed",
    );

  private chooseProjectPath = async (
    mode: "open" | "save",
  ): Promise<string | null> => {
    this.patch({ projectBusy: true });

    try {
      const project = this.context.getSnapshot().project;
      const currentPath = project?.path || project?.recent[0]?.path || "";
      const suggestedName = project?.name || "Untitled";

      let status = await this.context.api.beginProjectDialog({
        mode,
        currentPath,
        suggestedName,
      });

      while (status.state === "pending") {
        await new Promise((resolve) => setTimeout(resolve, 120));
        status = await this.context.api.getProjectDialog(status.id);
      }

      this.patch({ projectBusy: false });

      return status.state === "done" ? status.path : null;
    } catch (error) {
      this.patch({ projectBusy: false });
      this.notify(
        failureToast(
          "Could not open the file chooser",
          error,
          "project-dialog",
        ),
      );
      return null;
    }
  };

  private run = async (
    command: ControlProjectCommand,
    reloadState: boolean,
    failureTitle: string,
    successTitle: string,
  ): Promise<boolean> => {
    this.patch({ projectBusy: true });

    try {
      const project = await this.context.api.updateProject(command);
      const state = reloadState
        ? await this.context.api.getState()
        : this.context.getSnapshot().state;

      this.patch({
        project,
        projectBusy: false,
        ...(state !== null
          ? {
              phase: "ready" as const,
              state: { ...state, project },
              updatedAt: this.context.now(),
            }
          : {}),
      });

      this.notify({
        severity: "success",
        title: successTitle,
        ...(project.name.length > 0 ? { detail: project.name } : {}),
        key: "project",
      });

      return true;
    } catch (error) {
      this.patch({ projectBusy: false });
      this.notify(failureToast(failureTitle, error, "project"));
      return false;
    }
  };

  private notify(input: Parameters<ActionContext["notify"]>[0]): void {
    this.context.notify(input);
  }

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
