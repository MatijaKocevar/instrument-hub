import { renderToStaticMarkup } from "react-dom/server";

import { createControlApi, type ControlPluginList } from "../../api/client";
import { sourceRequestForPlugin } from "../../features/instrument";
import {
    PluginsPage,
    filterPlugins,
    formatScanTime,
    pluginAssignments,
    pluginAssignmentsFromState,
    pluginCountLabel,
    pluginKind,
    usedByPlugin,
} from "../../features/plugins";
import { AppStore } from "../../state/store";
import { startFixtureServer } from "../fixtureServer";
import { FIXTURE, PLUGIN_FIXTURE } from "../fixtures";
import { check, countOf, renderApp, writeArtifact } from "../harness";

export function pluginsModelTest (): void
{
    const plugins = PLUGIN_FIXTURE.plugins;

    check (filterPlugins (plugins, "").length === 3, "an empty search lists every plug-in");
    check (filterPlugins (plugins, "   ").length === 3, "a blank search changes nothing");
    check (filterPlugins (plugins, "arturia").length === 1
               && filterPlugins (plugins, "arturia")[0]?.name === "Analog Lab V",
           "search matches the vendor");
    check (filterPlugins (plugins, "TESTPLUGIN").length === 1
               && filterPlugins (plugins, "TESTPLUGIN")[0]?.name === "TestPlugin",
           "search is case-insensitive");
    check (filterPlugins (plugins, "instrument hub test").length === 2,
           "every search term must match, narrowing the list");
    check (filterPlugins (plugins, "fx").length === 1
               && filterPlugins (plugins, "fx")[0]?.name === "TestReverb",
           "search matches the category");
    check (filterPlugins (plugins, "vst3 testreverb").length === 1,
           "search matches the format and bundle path");
    check (filterPlugins (plugins, "no-such-plugin").length === 0, "an unmatched search is empty");

    const assignments = pluginAssignmentsFromState (FIXTURE);

    check (assignments.length === 1, "only plugin-backed patches count as assignments");
    check (assignments[0]?.slot === 6 && assignments[0]?.patchName === "Celesta",
           "the assignment names its patch and slot");
    check (usedByPlugin (assignments, plugins[2]!).length === 1,
           "Analog Lab is used by its patch");
    check (usedByPlugin (assignments, plugins[0]!).length === 0,
           "an unassigned plug-in has no usage entries");
    check (pluginAssignments (FIXTURE.families.flatMap (family => family.patches)).length === 1,
           "assignments can be derived from a flat patch list");

    check (pluginKind (plugins[0]!) === "Instrument", "an instrument plug-in is labelled Instrument");
    check (pluginKind (plugins[1]!) === "Effect", "an effect plug-in is labelled Effect");
    check (pluginCountLabel (3, 3) === "3 plug-ins", "a full list shows only the total");
    check (pluginCountLabel (1, 3) === "1 of 3 plug-ins", "a filtered list shows both counts");
    check (pluginCountLabel (1, 1) === "1 plug-in", "the singular form is used for one plug-in");
    check (formatScanTime (0) === "never" && formatScanTime (Number.NaN) === "never",
           "a missing scan time reads as never");
    check (formatScanTime (1_700_000_000_000) !== "never",
           "a real scan timestamp is formatted");
}

export async function pluginsPageTest (): Promise<void>
{
    const server = await startFixtureServer ();

    try
    {
        const store = new AppStore (
            createControlApi ({ baseUrl: server.baseUrl }),
            { pollIntervalMs: 60_000 });

        check (await store.refresh (), "the plugins fixture loads GET /state");
        check (await store.loadPlugins (), "GET /plugins returns the installed inventory");

        const inventory = store.getSnapshot ().plugins;

        check (inventory !== null && inventory.total === 3,
               "the inventory reports three installed plug-ins");

        const initial = renderApp (store, "/plugins");

        writeArtifact ("ui-plugins-snapshot.html", initial);

        check (initial.includes ('data-testid="plugins-page"'), "the /plugins route renders the page");
        check (countOf (initial, 'data-testid="plugin-row"') === 3,
               "the page lists every scanned plug-in");
        check (initial.includes ('data-testid="plugin-name">TestPlugin<'),
               "the page shows TestPlugin by name");
        check (initial.includes ('data-testid="plugin-name">TestReverb<'),
               "the page shows the scanned effect");
        check (initial.includes ('data-testid="plugin-name">Analog Lab V<'),
               "the page shows a third-party plug-in");
        check (initial.includes ('data-testid="plugin-vendor">Arturia<'),
               "the page shows the vendor");
        check (initial.includes ('data-testid="plugin-version">5.10.0<'),
               "the page shows the version");
        check (initial.includes ('data-testid="plugin-category">Instrument<'),
               "the page shows the category");
        check (initial.includes ('data-testid="plugin-format">VST3<'),
               "the page shows the format");
        check (initial.includes ("/usr/lib/vst3/TestPlugin.vst3"),
               "the page shows the bundle path");
        check (initial.includes ('data-instrument="true" data-effect="false">Instrument<'),
               "an instrument row is labelled as an instrument");
        check (initial.includes ('data-instrument="false" data-effect="true">Effect<'),
               "an effect row is labelled as an effect");

        check (countOf (initial, 'data-testid="plugin-used-by"') === 1,
               "only the assigned plug-in gets a used-by hint");
        check (initial.includes ('data-testid="plugin-used-by" data-count="1"'),
               "the used-by hint counts its patches");
        check (initial.includes ('data-patch="Celesta"')
                   && initial.includes ("slot 07 · Keys"),
               "the used-by hint names the patch, slot and family");
        check (countOf (initial, 'data-testid="plugin-unused"') === 2,
               "unassigned plug-ins are marked as unused");

        check (initial.includes ('data-testid="plugin-search"'), "the page has a search field");
        check (initial.includes ('data-testid="plugin-rescan"'), "the page has a rescan button");
        check (initial.includes ("3 plug-ins"), "the page reports the inventory size");
        check (! initial.includes ('data-testid="plugin-assign"'),
               "the page has no assignment control");
        check (! initial.includes ("<select"), "the page has no plug-in picker");
        check (initial.includes ("This page only reads the scan"),
               "the page says assignment belongs to the patch");

        const assignments = pluginAssignmentsFromState (store.getSnapshot ().state);
        const filtered = renderToStaticMarkup (
            <PluginsPage
                inventory={inventory}
                assignments={assignments}
                initialSearch="arturia"
            />);

        check (countOf (filtered, 'data-testid="plugin-row"') === 1,
               "search filters the visible rows");
        check (filtered.includes ('data-testid="plugin-name">Analog Lab V<'),
               "the matching plug-in stays visible");
        check (! filtered.includes ('data-testid="plugin-name">TestPlugin<'),
               "non-matching plug-ins are filtered out");
        check (filtered.includes ("1 of 3 plug-ins"), "the filtered count shows 1 of 3");

        const multiTerm = renderToStaticMarkup (
            <PluginsPage inventory={inventory} initialSearch="vst3 testreverb" />);

        check (countOf (multiTerm, 'data-testid="plugin-row"') === 1
                   && multiTerm.includes ('data-testid="plugin-name">TestReverb<'),
               "multi-term search matches the format and name together");

        const noMatch = renderToStaticMarkup (
            <PluginsPage inventory={inventory} initialSearch="no-such-plugin" />);

        check (noMatch.includes ('data-testid="plugin-empty"'), "an unmatched search shows an empty state");
        check (noMatch.includes ("No plug-ins match"), "the empty state repeats the query");
        check (countOf (noMatch, 'data-testid="plugin-row"') === 0, "an unmatched search lists no rows");

        check (await store.changeInstrumentSource (3, sourceRequestForPlugin (PLUGIN_FIXTURE.plugins[0]!)),
               "assigning TestPlugin to a patch succeeds");

        const assigned = renderApp (store, "/plugins");

        check (countOf (assigned, 'data-testid="plugin-used-by"') === 2,
               "a second assignment adds a second used-by hint");
        check (/data-plugin-id="com.instrumenthub.TestPlugin"[^>]*data-used-by="1"/.test (assigned),
               "the TestPlugin row now reports one user");
        check (assigned.includes ('data-patch="Drawbar Organ"'),
               "the used-by hint follows the newly assigned patch");

        check (await store.rescanPlugins (), "POST /plugins/rescan starts a scan");
        check (server.pluginRequests.includes ("rescan"), "the rescan reached the engine");
        check (store.getSnapshot ().plugins?.scanning === true,
               "the store adopts the scanning state");

        const scanning = renderApp (store, "/plugins");

        check (scanning.includes ('data-testid="plugins-page" data-count="3" data-total="3" data-scanning="true"'),
               "the page reports that a scan is running");
        check (scanning.includes ('data-testid="plugin-scan-state" data-scanning="true"'),
               "the scan badge turns on");
        check (scanning.includes ("Scanning…"), "the scan badge is readable");
        check (/data-testid="plugin-rescan"[^>]*disabled/.test (scanning),
               "the rescan button waits for the running scan");

        check (await store.loadPlugins (), "the polled inventory read succeeds");
        check (store.getSnapshot ().plugins?.scanning === false, "the scan settles");
        check (store.getSnapshot ().plugins?.plugins.some (plugin => plugin.name === "TestDelay") === true,
               "the rescan picked up the newly installed plug-in");

        const rescanned = renderApp (store, "/plugins");

        check (countOf (rescanned, 'data-testid="plugin-row"') === 4,
               "the refreshed page lists the new plug-in");
        check (rescanned.includes ('data-testid="plugin-name">TestDelay<'),
               "the newly scanned plug-in is visible");
        check (rescanned.includes ("4 plug-ins"), "the count grows with the rescan");
        check (rescanned.includes ('data-scanning="false"'), "the page leaves its scanning state");

        const failing = new AppStore (
            createControlApi ({ baseUrl: "http://127.0.0.1:1" }),
            { pollIntervalMs: 60_000 });

        check (! await failing.loadPlugins (), "a failed inventory read is reported");
        check (failing.getSnapshot ().pluginError !== null, "the store keeps the inventory error");

        const failureHtml = renderApp (failing, "/plugins");

        check (failureHtml.includes ('data-testid="plugins-page"'), "the page still renders on failure");
        check (failureHtml.includes ('data-testid="plugin-error"'), "the failure is shown");
        check (failureHtml.includes ('data-testid="plugin-retry"'), "the failure offers a retry");
        check (countOf (failureHtml, 'data-testid="plugin-row"') === 0,
               "a failed load lists no stale rows");
    }
    finally
    {
        await server.close ();
    }
}

export function assertSnapshotPlugins (pluginsHtml: string, inventory: ControlPluginList | null): void
{
    check (pluginsHtml.includes ('data-testid="plugins-page"'), "the /plugins route resolves");
    check (pluginsHtml.includes ('data-testid="plugin-search"'),
           "the plugins page has a search control");
    check (pluginsHtml.includes ('data-testid="plugin-rescan"'),
           "the plugins page has a rescan control");
    check (countOf (pluginsHtml, 'data-testid="plugin-row"') === (inventory?.plugins.length ?? 0),
           "the plugins page lists every scanned plug-in");
    check (pluginsHtml.includes ('data-testid="plugin-count"'),
           "the plugins page reports how many plug-ins it lists");
    check (! pluginsHtml.includes ('data-testid="plugin-assign"'),
           "the plugins page exposes no assignment control");
}

export function assertSnapshotPluginsFixture (pluginsHtml: string): void
{
    check (pluginsHtml.includes ('data-testid="plugin-name">TestPlugin<'),
           "the plugins page shows the scanned TestPlugin");
    check (pluginsHtml.includes ('data-testid="plugin-vendor">Instrument Hub<'),
           "a plug-in row shows its vendor");
    check (pluginsHtml.includes ('data-testid="plugin-format">VST3<'),
           "a plug-in row shows its format");
    check (pluginsHtml.includes ('data-testid="plugin-path"')
               && pluginsHtml.includes ("/usr/lib/vst3/TestPlugin.vst3"),
           "a plug-in row shows its bundle path");
    check (pluginsHtml.includes ('data-testid="plugin-used-by"'),
           "a plug-in assigned to a patch shows the used-by hint");
    check (pluginsHtml.includes ('data-patch="Celesta"'),
           "the used-by hint names the patch that uses the plug-in");
    check (pluginsHtml.includes ('data-testid="plugin-unused"'),
           "an unassigned plug-in says it is not used by a patch");
}
