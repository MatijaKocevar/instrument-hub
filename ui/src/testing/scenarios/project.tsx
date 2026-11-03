import { createControlApi } from "../../api/client";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { check, countOf, renderApp } from "../harness";
import { sectionOf } from "../markup";

export async function projectTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    await store.refresh();

    const html = renderApp(store, "/grid", 1200);
    const footer = sectionOf(html, 'data-testid="app-footer"', "</footer>");

    check(
      footer.includes('data-testid="project-name"') && html.includes(">Demo<"),
      "the footer shows the open project name",
    );
    check(
      footer.includes('data-testid="project-dirty" data-dirty="false"'),
      "a clean project shows no dirty marker",
    );
    check(
      footer.includes('data-testid="project-save"') &&
        footer.includes('data-testid="project-open"'),
      "the footer offers Save and Open actions",
    );
    check(
      countOf(footer, 'data-testid="project-recent"') === 2,
      "the Open menu lists the recent projects",
    );

    await store.updatePatch(3, { volumeDb: -9 });

    const dirty = renderApp(store, "/grid", 1200);

    check(
      dirty.includes('data-testid="project-dirty" data-dirty="true"'),
      "an accepted edit shows the dirty marker",
    );
    check(
      store.getSnapshot().state?.project?.dirty === true,
      "the engine reports the project dirty",
    );

    await store.saveProject();

    const saved = renderApp(store, "/grid", 1200);

    check(
      saved.includes('data-testid="project-dirty" data-dirty="false"'),
      "Save clears the dirty marker",
    );
    check(
      server.projectRequests.some((request) => request.action === "save"),
      "Save reaches the project endpoint",
    );

    await store.openProject("/home/demo/Music/Instrument Hub/Rehearsal.ihub");

    const opened = renderApp(store, "/grid", 1200);

    check(
      opened.includes(">Rehearsal<"),
      "opening a project updates the footer name",
    );
    check(
      server.projectRequests.some(
        (request) =>
          request.action === "open" && request.path.endsWith("/Rehearsal.ihub"),
      ),
      "Open sends the recent path to the project endpoint",
    );

    await store.newProject("/home/demo/Music/Instrument Hub/Fresh.ihub");

    check(
      renderApp(store, "/grid", 1200).includes(">Fresh<"),
      "New updates the footer name",
    );

    await store.saveProjectAs("/home/demo/Music/Instrument Hub/Copy.ihub");

    const copied = renderApp(store, "/grid", 1200);

    check(copied.includes(">Copy<"), "Save as updates the footer name");
    check(
      server.projectRequests.some(
        (request) =>
          request.action === "save-as" && request.path.endsWith("/Copy.ihub"),
      ),
      "Save as sends the new path to the project endpoint",
    );

    await store.closeProject();

    const closed = renderApp(store, "/grid", 1200);

    check(closed.includes(">No project<"), "Close clears the footer name");
    check(
      closed.includes('data-testid="project-dirty" data-dirty="false"'),
      "Close shows no dirty marker",
    );
  } finally {
    await server.close();
  }
}
