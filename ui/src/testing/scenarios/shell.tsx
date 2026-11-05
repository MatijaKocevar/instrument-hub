import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

import { createControlApi } from "../../api/client";
import { armAndOpenInstrument } from "../../features/shell";
import { NavLink } from "../../features/shell/NavLink";
import { AUDIO_ITEM } from "../../features/shell/navigation";
import {
  instrumentPathForSlot,
  pathForRoute,
  routeFromPath,
  SHELL_ROUTES,
  slotFromSearch,
} from "../../state/router";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { check, countOf, renderApp, writeArtifact } from "../harness";
import { sectionOf } from "../markup";

export function routeTest(): void {
  check(
    SHELL_ROUTES.includes("grid") &&
      SHELL_ROUTES.includes("instrument") &&
      SHELL_ROUTES.includes("plugins") &&
      SHELL_ROUTES.includes("audio") &&
      SHELL_ROUTES.includes("settings") &&
      SHELL_ROUTES.includes("remote"),
    "the shell declares every page route",
  );

  for (const route of SHELL_ROUTES)
    check(
      routeFromPath(pathForRoute(route)) === route,
      `/${route} resolves to itself`,
    );

  check(
    routeFromPath("/") === "grid" && routeFromPath("/nope") === "grid",
    "the root and unknown paths land on the grid",
  );
  check(
    routeFromPath("/audio/") === "audio",
    "a trailing slash still resolves",
  );
  check(
    instrumentPathForSlot(3) === "/instrument?slot=3",
    "an instrument selection is encoded in the URL",
  );
  check(
    instrumentPathForSlot(null) === "/instrument",
    "no selection still resolves the instrument page",
  );
  check(
    slotFromSearch("?slot=12") === 12,
    "the instrument page reads its slot",
  );
  check(
    slotFromSearch("?code=123456") === null,
    "an unrelated query selects no patch",
  );
  check(
    slotFromSearch("?slot=-1") === null,
    "a negative slot is not a selection",
  );

  const actions: string[] = [];

  armAndOpenInstrument(
    5,
    (slot) => {
      actions.push(`arm:${String(slot)}`);
    },
    (path) => {
      actions.push(`nav:${path}`);
    },
  );

  check(
    actions.join("|") === "arm:5|nav:/instrument?slot=5",
    "one click arms the patch and routes to its instrument page",
  );

  const clicked: string[] = [];
  const sidebarLink = NavLink({
    item: AUDIO_ITEM,
    route: "grid",
    navigate: (path) => {
      clicked.push(path);
    },
    variant: "sidebar",
  });

  const onClick = (
    sidebarLink.props as {
      readonly onClick: (event: { preventDefault: () => void }) => void;
    }
  ).onClick;

  onClick({ preventDefault: () => undefined });

  check(
    clicked.join("|") === "/audio",
    "clicking the sidebar Audio & MIDI entry opens /audio",
  );
}

export async function shellTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    await store.refresh();

    const wide = renderApp(store, "/grid", 1200);

    writeArtifact("ui-shell-1200x800.html", wide);

    const wideNav = sectionOf(wide, 'data-testid="app-nav"', "</nav>");
    const wideSettingsNav = sectionOf(
      wide,
      'data-testid="settings-nav"',
      "</nav>",
    );
    const wideFooter = sectionOf(wide, 'data-testid="app-footer"', "</footer>");

    check(
      wide.includes('data-layout="wide"'),
      "a 1200 px viewport uses the wide shell",
    );
    check(
      wide.includes('data-viewport-width="1200"'),
      "the snapshot records the 1200 px viewport",
    );
    check(
      wide.includes('data-testid="app-sidebar"'),
      "the wide shell has a persistent left sidebar",
    );
    check(
      !wide.includes('data-testid="app-bottom-tabs"'),
      "the wide shell has no bottom tabs",
    );
    check(
      countOf(wide, 'data-testid="app-nav"') === 1,
      "exactly one workspace navigation is rendered",
    );
    check(
      countOf(wideNav, 'data-workspace-item="true"') === 3,
      "the sidebar lists Grid, Instrument and Plugins",
    );
    check(
      !wideNav.includes('data-testid="nav-audio"'),
      "Audio & MIDI is not a workspace item",
    );
    check(
      countOf(wideSettingsNav, 'data-testid="nav-audio"') === 1,
      "the Settings & Account group carries the Audio & MIDI entry",
    );
    check(
      wideSettingsNav.includes('href="/audio"'),
      "the sidebar Audio & MIDI entry routes to /audio",
    );
    check(
      !wideFooter.includes('data-testid="nav-audio"'),
      "the footer no longer carries the Audio & MIDI entry",
    );
    check(
      wide.includes('data-testid="settings-group"'),
      "the sidebar bottom group holds Settings & Account",
    );
    check(
      wide.includes("Settings &amp; Account"),
      "the bottom group is named in the sidebar",
    );
    check(
      wide.includes('data-testid="nav-grid" data-active="true"'),
      "the active route is highlighted on /grid",
    );
    check(
      wideSettingsNav.includes(">Audio &amp; MIDI<"),
      "the sidebar entry is named Audio & MIDI",
    );
    check(
      !wide.includes("app__nav-link"),
      "no in-page route pill replaces the sidebar",
    );
    check(
      !wide.includes("Back to the grid"),
      "page bodies carry no route back-buttons",
    );

    const destinations = [
      { path: "/grid", route: "grid", marker: 'data-testid="instrument-grid"' },
      {
        path: "/instrument",
        route: "instrument",
        marker: 'data-testid="instrument-page"',
      },
      {
        path: "/plugins",
        route: "plugins",
        marker: 'data-testid="plugins-page"',
      },
      { path: "/audio", route: "audio", marker: 'data-testid="audio-page"' },
      {
        path: "/settings",
        route: "settings",
        marker: 'data-testid="settings-page"',
      },
    ] as const;

    for (const destination of destinations) {
      const page = renderApp(store, destination.path, 1200);

      check(
        page.includes(destination.marker),
        `the ${destination.route} destination renders its page`,
      );
      check(
        page.includes(`data-route="${destination.route}"`),
        `the ${destination.route} destination is mounted`,
      );
      check(
        page.includes(
          `data-testid="nav-${destination.route}" data-active="true"`,
        ),
        `the ${destination.route} destination highlights its nav item`,
      );
      check(
        page.includes('data-testid="sidebar-session"'),
        `the ${destination.route} page keeps connection state in the sidebar`,
      );
      check(
        page.includes('data-testid="app-status"'),
        `the ${destination.route} page keeps connection state in the footer`,
      );
      check(
        !page.includes("app__header") &&
          !page.includes('data-testid="connection-status"'),
        `the ${destination.route} page has no global header or connection bar`,
      );
      check(
        countOf(page, 'data-testid="nav-grid"') === 1,
        `the ${destination.route} page has no duplicate Grid route button`,
      );
    }

    const settings = renderApp(store, "/settings", 1200);

    check(
      settings.includes('data-testid="pairing"'),
      "settings exposes the pairing code card",
    );
    check(
      settings.includes('data-testid="lan-toggle"'),
      "settings exposes LAN mode enable/disable",
    );
    check(
      settings.includes('data-testid="settings-revoke-token"'),
      "settings exposes token revocation",
    );
    check(
      settings.includes('data-testid="settings-account"'),
      "settings exposes the account section",
    );
    check(
      settings.includes('data-testid="settings-about"'),
      "settings exposes the About/version section",
    );
    check(
      settings.includes('data-testid="settings-version"'),
      "the About section carries the version",
    );

    const narrow = renderApp(store, "/plugins", 420);

    writeArtifact("ui-shell-narrow.html", narrow);

    const narrowTabs = sectionOf(
      narrow,
      'data-testid="app-bottom-tabs"',
      "</nav>",
    );

    check(
      narrow.includes('data-layout="narrow"'),
      "a 420 px viewport collapses the shell",
    );
    check(
      narrow.includes('data-testid="app-bottom-tabs"'),
      "the narrow shell uses bottom tabs",
    );
    check(
      !narrow.includes('data-testid="app-sidebar"'),
      "the narrow shell drops the full sidebar",
    );
    check(
      countOf(narrowTabs, 'data-workspace-item="true"') === 4,
      "the bottom tabs keep four workspace destinations",
    );
    check(
      countOf(narrowTabs, 'data-testid="nav-audio"') === 1,
      "the narrow bottom tabs keep the Audio & MIDI entry",
    );
    check(
      narrow.includes('data-testid="nav-settings"'),
      "the bottom tabs keep Settings & Account",
    );
    check(
      narrow.includes('data-testid="nav-plugins" data-active="true"'),
      "the active tab is highlighted on /plugins",
    );

    for (const route of ["grid", "instrument", "plugins", "audio", "settings"])
      check(
        countOf(narrowTabs, `data-testid="nav-${route}"`) === 1,
        `the narrow shell has one ${route} tab`,
      );

    const remote = renderApp(store, "/remote", 1200);

    check(
      remote.includes('data-testid="remote"'),
      "the /remote route renders the full-screen remote",
    );
    check(
      !remote.includes('data-testid="app-sidebar"') &&
        !remote.includes('data-testid="app-bottom-tabs"'),
      "the remote has no desktop shell navigation",
    );

    const css = readFileSync(
      fileURLToPath(new URL("../src/styles.css", import.meta.url)),
      "utf8",
    );

    check(
      /@media \(max-width: 900px\)/.test(css),
      "the shell has a narrow-width breakpoint",
    );
    check(
      /\.shell__tabs\s*\{/.test(css),
      "the collapsed shell styles bottom tabs",
    );
    check(
      /\.shell__sidebar\s*\{[^}]*position:\s*sticky/s.test(css),
      "the sidebar stays visible while a page scrolls",
    );
  } finally {
    await server.close();
  }
}

export async function clickArmsAndRoutesTest(): Promise<void> {
  const server = await startFixtureServer();

  try {
    const store = new AppStore(createControlApi({ baseUrl: server.baseUrl }), {
      pollIntervalMs: 60_000,
    });

    await store.refresh();

    let navigated = "";
    let armResult: Promise<boolean> = Promise.resolve(false);

    armAndOpenInstrument(
      5,
      (slot) => {
        armResult = store.armSlot(slot);
      },
      (path) => {
        navigated = path;
      },
    );

    check(
      navigated === "/instrument?slot=5",
      "clicking a cell routes to /instrument?slot=5",
    );
    check(
      store.getSnapshot().state?.armedSlot === 5,
      "clicking a cell arms the patch immediately",
    );

    const optimistic = renderApp(store);
    check(
      optimistic.includes('data-slot="5" data-armed="true"'),
      "the clicked cell is red on the next render",
    );

    check(await armResult, "POST /arm confirms the clicked patch");
    check(
      server.armRequests.length === 1 && server.armRequests[0]?.slot === 5,
      "the engine received the clicked slot",
    );
  } finally {
    await server.close();
  }
}

export function assertSnapshotNonBlank(html: string): void {
  check(html.length > 2000, "snapshot is non-blank");
}

export function assertSnapshotShell(html: string): void {
  const nav = sectionOf(html, 'data-testid="app-nav"', "</nav>");
  const settingsNav = sectionOf(html, 'data-testid="settings-nav"', "</nav>");
  const footer = sectionOf(html, 'data-testid="app-footer"', "</footer>");

  check(
    html.includes('data-testid="sidebar-session" data-phase="ready"'),
    "the sidebar reports the connection is ready",
  );
  check(html.includes("Connected"), "snapshot shows the connected status");
  check(html.includes("Instrument Hub"), "snapshot shows the application name");
  check(html.includes("0.1.0"), "snapshot shows the application version");
  check(
    html.includes('data-testid="app-status" data-phase="ready"'),
    "the footer reports the connection phase",
  );
  check(
    !html.includes("app__header") &&
      !html.includes('data-testid="connection-status"'),
    "the grid page has no global header or connection bar",
  );
  check(
    !html.includes("app__panels"),
    "the grid page renders the grid without engine panels",
  );
  check(
    html.includes('data-testid="instrument-grid"'),
    "snapshot shows the instrument grid",
  );
  check(
    html.includes('data-testid="app-sidebar"'),
    "snapshot shows the persistent sidebar",
  );
  check(
    html.includes('data-testid="app-nav"'),
    "snapshot shows the workspace navigation",
  );
  check(
    countOf(nav, 'data-workspace-item="true"') === 3,
    "the sidebar lists three workspace items",
  );
  check(
    !nav.includes('data-testid="nav-audio"'),
    "Audio & MIDI is not a workspace item",
  );
  check(
    settingsNav.includes('data-testid="nav-audio"'),
    "the Settings & Account group carries the Audio & MIDI entry",
  );
  check(
    !footer.includes('data-testid="nav-audio"'),
    "the footer no longer carries the Audio & MIDI entry",
  );
  check(
    html.includes(">Grid<") &&
      html.includes(">Instrument<") &&
      html.includes(">Plugins<") &&
      settingsNav.includes("Audio &amp; MIDI"),
    "the workspace items and the settings group list every destination",
  );
  check(
    html.includes('data-testid="settings-group"'),
    "the sidebar has a Settings & Account group",
  );
  check(
    html.includes("Settings &amp; Account"),
    "the bottom group is labelled Settings & Account",
  );
  check(
    html.includes('href="/settings" data-testid="nav-settings"'),
    "the Settings & Account item routes to /settings",
  );
  check(
    html.includes('data-testid="nav-grid" data-active="true"'),
    "the Grid tab is active on /grid",
  );
  check(
    countOf(html, 'data-testid="nav-grid"') === 1,
    "the shell renders one Grid route button",
  );
}

export function assertSnapshotActiveNav(
  audioHtml: string,
  settingsHtml: string,
): void {
  check(
    audioHtml.includes('data-testid="nav-audio" data-active="true"'),
    "the Audio & MIDI item is active on /audio",
  );
  check(
    settingsHtml.includes('data-testid="nav-settings" data-active="true"'),
    "the Settings & Account item is active on /settings",
  );
}

export function assertSnapshotFooter(html: string, armedSlot: number): void {
  const sidebar = sectionOf(html, 'data-testid="app-sidebar"', "</aside>");

  check(
    armedSlot < 0 === html.includes("armed none"),
    "the footer reports the fetched arm state",
  );
  check(
    sidebar.includes('data-testid="project-name"') && html.includes(">Demo<"),
    "the sidebar shows the project name",
  );
  check(
    sidebar.includes('data-testid="project-dirty" data-dirty="false"'),
    "a clean project carries a cleared dirty marker",
  );
  check(
    sidebar.includes('data-testid="project-save"') &&
      sidebar.includes('data-testid="project-open"'),
    "the sidebar offers the Save and Open actions",
  );
}
