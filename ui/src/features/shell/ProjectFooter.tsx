import { useState, type ReactElement } from "react";

import type { ShellData } from "./useShellData";

export function ProjectFooter({
  shell,
}: {
  readonly shell: ShellData;
}): ReactElement {
  const { app, state, store } = shell;
  const project = state?.project;
  const recent = app.project?.recent ?? [];
  const path = project?.path ?? "";
  const name = project?.name ?? "";
  const dirty = project?.dirty === true;
  const busy = app.projectBusy;
  const [draftPath, setDraftPath] = useState(path);

  const openPath = (): void => {
    if (draftPath.length > 0) void store.openProject(draftPath);
  };

  const newProject = (): void => {
    if (draftPath.length > 0) void store.newProject(draftPath);
    else void store.newProject();
  };

  const saveAs = (): void => {
    if (draftPath.length > 0) void store.saveProjectAs(draftPath);
  };

  return (
    <div className="app__project" data-testid="project">
      <span className="app__project-name" data-testid="project-name">
        {name.length > 0 ? name : "No project"}
      </span>
      <span
        className="app__project-dirty"
        data-testid="project-dirty"
        data-dirty={dirty ? "true" : "false"}
        aria-label={dirty ? "Unsaved changes" : "Saved"}
      >
        {dirty ? "●" : ""}
      </span>
      <button
        type="button"
        className="app__project-action"
        data-testid="project-save"
        disabled={busy || path.length === 0}
        onClick={() => {
          void store.saveProject();
        }}
      >
        Save
      </button>
      <details className="app__project-open" data-testid="project-open">
        <summary className="app__project-action">Open</summary>
        <div className="app__project-menu">
          {recent.length > 0 ? (
            <ul
              className="app__project-recent"
              data-testid="project-recent-list"
            >
              {recent.map((entry) => (
                <li key={entry.path}>
                  <button
                    type="button"
                    className="app__project-recent-item"
                    data-testid="project-recent"
                    data-path={entry.path}
                    disabled={busy}
                    onClick={() => {
                      setDraftPath(entry.path);
                      void store.openProject(entry.path);
                    }}
                  >
                    {entry.name}
                  </button>
                </li>
              ))}
            </ul>
          ) : (
            <p className="app__project-empty">No recent projects</p>
          )}
          <input
            className="app__project-path"
            data-testid="project-path"
            value={draftPath}
            placeholder="/path/to/project.ihub"
            onChange={(event) => {
              setDraftPath(event.target.value);
            }}
          />
          <div className="app__project-actions">
            <button
              type="button"
              className="app__project-action"
              data-testid="project-open-path"
              disabled={busy || draftPath.length === 0}
              onClick={openPath}
            >
              Open path
            </button>
            <button
              type="button"
              className="app__project-action"
              data-testid="project-new"
              disabled={busy}
              onClick={newProject}
            >
              New
            </button>
            <button
              type="button"
              className="app__project-action"
              data-testid="project-save-as"
              disabled={busy || draftPath.length === 0}
              onClick={saveAs}
            >
              Save as…
            </button>
            <button
              type="button"
              className="app__project-action"
              data-testid="project-close"
              disabled={busy || path.length === 0}
              onClick={() => {
                void store.closeProject();
              }}
            >
              Close
            </button>
          </div>
          {app.projectError !== null ? (
            <p className="app__project-error" data-testid="project-error">
              {app.projectError}
            </p>
          ) : null}
        </div>
      </details>
    </div>
  );
}
