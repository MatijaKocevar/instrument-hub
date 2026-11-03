import type { ControlProjectCommand } from "../api/client";
import type { ActionContext } from "./actionContext";
import { messageOf } from "./errors";

export class ProjectActions {
  constructor(private readonly context: ActionContext) {}

  loadProject = async (): Promise<boolean> => {
    try {
      const project = await this.context.api.getProject();

      this.patch({ project, projectError: null });
      return true;
    } catch (error) {
      this.patch({ projectError: messageOf(error) });
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
    );

  openProject = (path: string): Promise<boolean> =>
    this.run({ action: "open", path }, true);

  saveProject = (): Promise<boolean> => this.run({ action: "save" });

  saveProjectAs = (path: string): Promise<boolean> =>
    this.run({ action: "save-as", path });

  closeProject = (): Promise<boolean> => this.run({ action: "close" });

  private run = async (
    command: ControlProjectCommand,
    reloadState = false,
  ): Promise<boolean> => {
    this.patch({ projectBusy: true, projectError: null });

    try {
      const project = await this.context.api.updateProject(command);
      const state = reloadState
        ? await this.context.api.getState()
        : this.context.getSnapshot().state;

      this.patch({
        project,
        projectBusy: false,
        projectError: null,
        ...(state !== null
          ? {
              phase: "ready" as const,
              state: { ...state, project },
              updatedAt: this.context.now(),
            }
          : {}),
      });

      return true;
    } catch (error) {
      this.patch({ projectBusy: false, projectError: messageOf(error) });
      return false;
    }
  };

  private patch(change: Parameters<ActionContext["patch"]>[0]): void {
    this.context.patch(change);
  }
}
