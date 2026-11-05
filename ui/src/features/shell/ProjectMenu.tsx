import { useEffect, useRef, useState, type ReactElement } from "react";

import type { ShellData } from "./useShellData";
import "./project-menu.css";

type ProjectPanel = "none" | "open" | "recent" | "saveAs";

export function ProjectMenu({
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
  const [open, setOpen] = useState(false);
  const [panel, setPanel] = useState<ProjectPanel>("none");
  const [draftPath, setDraftPath] = useState(path);
  const root = useRef<HTMLDivElement>(null);

  useEffect(() => {
    setDraftPath(path);
  }, [path]);

  useEffect(() => {
    if (!open) return;

    const dismiss = (event: PointerEvent): void => {
      if (
        root.current !== null &&
        !root.current.contains(event.target as Node)
      ) {
        setOpen(false);
        setPanel("none");
      }
    };

    const escape = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        setOpen(false);
        setPanel("none");
      }
    };

    document.addEventListener("pointerdown", dismiss);
    document.addEventListener("keydown", escape);

    return () => {
      document.removeEventListener("pointerdown", dismiss);
      document.removeEventListener("keydown", escape);
    };
  }, [open]);

  const closeMenu = (): void => {
    setOpen(false);
    setPanel("none");
  };

  const togglePanel = (next: ProjectPanel): void => {
    setPanel((current) => (current === next ? "none" : next));
  };

  const openPath = (): void => {
    if (draftPath.length > 0) void store.openProject(draftPath);
    closeMenu();
  };

  const saveAs = (): void => {
    if (draftPath.length > 0) void store.saveProjectAs(draftPath);
    closeMenu();
  };

  return (
    <div className="project-menu" data-testid="project" ref={root}>
      <button
        type="button"
        className="project-menu__trigger"
        data-testid="project-menu-button"
        aria-haspopup="menu"
        aria-expanded={open}
        onClick={() => {
          setOpen((current) => !current);
          setPanel("none");
        }}
      >
        <span className="project-menu__eyebrow">Project</span>
        <span className="project-menu__title">
          <span className="project-menu__name" data-testid="project-name">
            {name.length > 0 ? name : "No project"}
          </span>
          <span
            className="project-menu__dirty"
            data-testid="project-dirty"
            data-dirty={dirty ? "true" : "false"}
            aria-label={dirty ? "Unsaved changes" : "Saved"}
          >
            {dirty ? "●" : ""}
          </span>
        </span>
      </button>

      <div
        className="project-menu__popover"
        data-testid="project-menu"
        role="menu"
        hidden={!open}
      >
        <button
          type="button"
          className="project-menu__item"
          data-testid="project-new"
          role="menuitem"
          disabled={busy}
          onClick={() => {
            void store.newProject();
            closeMenu();
          }}
        >
          New project
        </button>
        <button
          type="button"
          className="project-menu__item"
          data-testid="project-open"
          role="menuitem"
          aria-expanded={panel === "open"}
          onClick={() => {
            togglePanel("open");
          }}
        >
          Open project…
        </button>
        <button
          type="button"
          className="project-menu__item"
          data-testid="project-save"
          role="menuitem"
          disabled={busy || path.length === 0}
          onClick={() => {
            void store.saveProject();
            closeMenu();
          }}
        >
          Save
        </button>
        <button
          type="button"
          className="project-menu__item"
          data-testid="project-recent-toggle"
          role="menuitem"
          aria-expanded={panel === "recent"}
          onClick={() => {
            togglePanel("recent");
          }}
        >
          Recent projects
          <span className="project-menu__chevron" aria-hidden="true">
            ›
          </span>
        </button>

        <div className="project-menu__divider" />

        <button
          type="button"
          className="project-menu__item"
          data-testid="project-save-as"
          role="menuitem"
          aria-expanded={panel === "saveAs"}
          onClick={() => {
            togglePanel("saveAs");
          }}
        >
          Save as…
        </button>
        <button
          type="button"
          className="project-menu__item"
          data-testid="project-close"
          role="menuitem"
          disabled={busy || path.length === 0}
          onClick={() => {
            void store.closeProject();
            closeMenu();
          }}
        >
          Close project
        </button>

        {app.projectError !== null ? (
          <p className="project-menu__error" data-testid="project-error">
            {app.projectError}
          </p>
        ) : null}

        <div
          className="project-menu__subpanel project-menu__subpanel--recent"
          data-testid="project-recent-panel"
          hidden={panel !== "recent"}
        >
          {recent.length > 0 ? (
            <ul
              className="project-menu__list"
              data-testid="project-recent-list"
            >
              {recent.map((entry) => (
                <li key={entry.path}>
                  <button
                    type="button"
                    className="project-menu__recent"
                    data-testid="project-recent"
                    data-path={entry.path}
                    disabled={busy}
                    onClick={() => {
                      void store.openProject(entry.path);
                      closeMenu();
                    }}
                  >
                    {entry.name}
                  </button>
                </li>
              ))}
            </ul>
          ) : (
            <p className="project-menu__empty">No recent projects</p>
          )}
        </div>

        <div
          className="project-menu__subpanel project-menu__subpanel--path"
          data-testid="project-path-panel"
          hidden={panel !== "open" && panel !== "saveAs"}
        >
          <span className="project-menu__panel-title">
            {panel === "saveAs" ? "Save project as" : "Open project"}
          </span>
          <input
            className="project-menu__path"
            data-testid="project-path"
            value={draftPath}
            placeholder="/path/to/project.ihub"
            onChange={(event) => {
              setDraftPath(event.target.value);
            }}
          />
          <div className="project-menu__panel-actions">
            <button
              type="button"
              className="project-menu__confirm"
              data-testid={
                panel === "saveAs"
                  ? "project-save-as-confirm"
                  : "project-open-path"
              }
              disabled={busy || draftPath.length === 0}
              onClick={panel === "saveAs" ? saveAs : openPath}
            >
              {panel === "saveAs" ? "Save as" : "Open"}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
