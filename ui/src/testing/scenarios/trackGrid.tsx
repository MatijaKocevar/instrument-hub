import { isValidElement, type ReactElement, type ReactNode } from "react";
import { renderToStaticMarkup } from "react-dom/server";

import {
  GridView,
  gridFromState,
  TRACK_VOLUME_STAGE_POSITIONS,
  volumeStepForRow,
  type GridTrack,
} from "../../features/grid";
import { TrackSelectButton } from "../../features/grid/TrackSelectButton";
import { TrackVolumeFader } from "../../features/grid/TrackVolumeFader";
import { TRACK_FIXTURE } from "../fixtures";
import { check, countOf, writeArtifact } from "../harness";

type ElementProps = Record<string, unknown>;

function isElement(node: ReactNode): node is ReactElement<ElementProps> {
  return isValidElement(node);
}

function elementsByTestId(
  node: ReactNode,
  testId: string,
): ReactElement<ElementProps>[] {
  const found: ReactElement<ElementProps>[] = [];

  const visit = (current: ReactNode): void => {
    if (Array.isArray(current)) {
      current.forEach(visit);
      return;
    }

    if (!isElement(current)) return;

    if (typeof current.type === "function") {
      const component = current.type as (props: ElementProps) => ReactNode;
      visit(component(current.props));
      return;
    }

    if (current.props["data-testid"] === testId) found.push(current);

    visit(current.props["children"] as ReactNode);
  };

  visit(node);

  return found;
}

function invoke(element: ReactElement<ElementProps>, handler: string): void {
  const callback = element.props[handler];

  if (typeof callback === "function") (callback as () => void)();
}

function renderFader(track: GridTrack): string {
  return renderToStaticMarkup(
    <TrackVolumeFader track={track} onSetTrackVolume={() => {}} />,
  );
}

function clickVolumeStages(track: GridTrack): Array<[number, number]> {
  const calls: Array<[number, number]> = [];
  const tree = TrackVolumeFader({
    track,
    onSetTrackVolume: (index, step) => {
      calls.push([index, step]);
    },
  });

  for (const button of elementsByTestId(tree, "track-volume"))
    invoke(button, "onClick");

  return calls;
}

export function trackGridModelTest(): void {
  const model = gridFromState(TRACK_FIXTURE);
  const html = renderToStaticMarkup(
    <GridView
      model={model}
      onSelectTrack={() => {}}
      onToggleTrackFx={() => {}}
      onSetTrackVolume={() => {}}
    />,
  );

  writeArtifact("ui-track-grid-snapshot.html", html);

  check(model.layout === "tracks", "a track project builds a track grid model");
  check(
    countOf(html, 'data-testid="track-grid-table"') === 1,
    "the track grid table renders",
  );
  check(
    countOf(html, 'data-testid="family"') === 0,
    "the track grid does not render the legacy family rows",
  );
  check(
    countOf(html, 'data-testid="track-column"') === 8,
    "eight channel strips render",
  );
  check(
    countOf(html, 'data-testid="track-cell"') === 64,
    "the track grid is eight tracks by eight rows",
  );
  check(
    countOf(html, 'data-testid="track-select"') === 8,
    "every channel strip has an instrument selector",
  );
  check(
    countOf(html, 'data-testid="track-hot-fx"') === 16,
    "every channel strip has two hardware FX switches",
  );
  check(
    countOf(html, 'data-testid="track-volume"') === 40,
    "every channel strip has five volume hit areas",
  );
  check(
    countOf(html, 'data-testid="track-fader"') === 8,
    "each track owns one continuous mixer fader",
  );
  check(
    countOf(html, 'data-testid="track-fader-rail"') === 8 &&
      countOf(html, 'data-testid="track-fader-fill"') === 8 &&
      countOf(html, 'data-testid="track-fader-cap"') === 8,
    "each fader renders one rail, fill and cap",
  );
  check(
    countOf(html, 'data-testid="track-fader-readout"') === 8,
    "each fader renders one dB readout",
  );
  check(
    countOf(html, 'data-testid="track-fader-tick"') === 40,
    "each fader renders five stage ticks",
  );
  check(
    countOf(html, 'data-testid="track-status"') === 8 &&
      countOf(html, 'data-testid="track-status" data-status="ok"') === 7 &&
      countOf(html, 'data-testid="track-status" data-status="empty"') === 1,
    "every track carries a status dot and the empty track stays idle",
  );
  check(
    countOf(html, 'data-testid="track-fx-switch"') === 16,
    "every FX row renders a switch",
  );
  check(
    html.includes(
      'data-testid="track-select" data-track-index="2" data-slot="2" aria-pressed="true"',
    ),
    "the selected track is marked in its selector",
  );
  check(
    html.includes(
      'data-testid="track-column" data-track-index="2" data-track-id="track-3" data-selected="true"',
    ),
    "the armed track gets one border for the whole channel strip",
  );
  check(
    countOf(html, 'data-testid="track-selection" data-selected="true"') === 1,
    "the selector card shows the selection state",
  );
  check(
    html.includes('data-hot="0" data-assigned="true"') &&
      html.includes(">EQ<") &&
      html.includes(">Compressor<"),
    "assigned hot FX show their insert names",
  );
  check(
    countOf(html, 'data-assigned="false"') === 30,
    "unassigned hot FX stay dark and inert",
  );
  check(
    countOf(html, 'data-testid="track-fx-state" data-state="on"') === 5 &&
      countOf(html, 'data-testid="track-fx-state" data-state="bypass"') === 1 &&
      countOf(html, 'data-testid="track-fx-state" data-state="unassigned"') ===
        8 &&
      countOf(html, 'data-testid="track-fx-state" data-state="inactive"') === 2,
    "FX state text names on, bypass, unassigned or inactive",
  );
  check(
    html.includes("Drawbar Organ") && html.includes("Analog Lab V"),
    "track instruments are visible in the selector cards",
  );

  for (const [index, label] of TRACK_VOLUME_STAGE_POSITIONS.entries()) {
    check(
      countOf(html, `data-position="${String(label)}"`) === 8,
      `stage ${String(index)} sits at ${String(label)}% on every fader`,
    );
  }

  for (const label of ["0%", "50%", "75%", "100%", "+3 dB"])
    check(
      countOf(html, `data-label="${label}"`) === 8,
      `every fader labels its ${label} stage`,
    );

  check(
    html.includes(
      'data-testid="track-fader-tick" data-stage="4" data-position="100" data-label="+3 dB"',
    ) && html.includes("track-grid__fader-tick--over"),
    "the +3 dB stage is amber",
  );

  const overTrack = model.tracks[4];

  if (overTrack !== undefined) {
    const overHtml = renderFader(overTrack);

    check(
      overHtml.includes(
        'data-testid="track-fader-fill" data-step="4" style="height:100%"',
      ) &&
        overHtml.includes(
          'data-testid="track-fader-cap" data-step="4" style="bottom:100%"',
        ),
      "the +3 dB track fills to the top detent and caps it",
    );
    check(
      overHtml.includes("track-grid__fader-fill--over") &&
        overHtml.includes("track-grid__fader-cap--over"),
      "the +3 dB fill and cap are amber",
    );
    check(
      overHtml.includes(
        'data-testid="track-fader-readout" data-track-index="4" data-volume-step="4" data-db="3.0"',
      ) && overHtml.includes(">3.0 dB<"),
      "the dB readout follows the +3 dB level",
    );
    check(
      overHtml.includes(
        'data-stage="4" data-position="100" data-label="+3 dB" data-active="true"',
      ),
      "the active detent is marked on the fader scale",
    );
  }

  const partialModel = gridFromState({
    ...TRACK_FIXTURE,
    tracks: (TRACK_FIXTURE.tracks ?? []).map((track, index) =>
      index === 0
        ? { ...track, volumeStep: 2, volumeDb: -2.5, gain: 0.75 }
        : track,
    ),
  });
  const partialTrack = partialModel.tracks[0];

  if (partialTrack !== undefined) {
    const partialHtml = renderFader(partialTrack);

    check(
      partialHtml.includes(
        'data-testid="track-fader-fill" data-step="2" style="height:50%"',
      ) &&
        partialHtml.includes(
          'data-testid="track-fader-cap" data-step="2" style="bottom:50%"',
        ),
      "a 75% track fills from the bottom to its active detent",
    );
    check(
      partialHtml.includes(
        'data-testid="track-fader-readout" data-track-index="0" data-volume-step="2" data-db="-2.5"',
      ) && partialHtml.includes(">-2.5 dB<"),
      "the fader readout follows the track level",
    );
  }

  const zeroModel = gridFromState({
    ...TRACK_FIXTURE,
    tracks: (TRACK_FIXTURE.tracks ?? []).map((track, index) =>
      index === 0 ? { ...track, volumeStep: 0, volumeDb: -60, gain: 0 } : track,
    ),
  });
  const zeroTrack = zeroModel.tracks[0];

  if (zeroTrack !== undefined)
    check(
      renderFader(zeroTrack).includes(
        'data-testid="track-fader-fill" data-step="0" style="height:0%"',
      ) &&
        renderFader(zeroTrack).includes(
          'data-testid="track-fader-cap" data-step="0" style="bottom:0%"',
        ),
      "the 0% stage leaves the rail empty and parks the cap at the bottom",
    );

  const wired = model.tracks.map(clickVolumeStages);
  const expectedSteps = [4, 3, 2, 1, 0];

  check(
    wired.every(
      (calls, trackIndex) =>
        calls.length === 5 &&
        calls.every(
          ([index, step], position) =>
            index === model.tracks[trackIndex]?.index &&
            step === expectedSteps[position],
        ),
    ),
    "all 40 volume rows are independent click targets wired to their own step",
  );
  check(
    expectedSteps.every((step) => volumeStepForRow(step) === step),
    "each volume row maps directly to its own step",
  );

  const selectCalls: number[] = [];
  const openCalls: number[] = [];
  const selectedTrack = model.tracks[2];

  if (selectedTrack !== undefined) {
    const buttonTree = TrackSelectButton({
      track: selectedTrack,
      onSelectTrack: (index) => {
        selectCalls.push(index);
      },
      onOpenTrack: (index) => {
        openCalls.push(index);
      },
    });
    const [selector] = elementsByTestId(buttonTree, "track-select");

    check(
      selector !== undefined &&
        typeof selector.props.onClick === "function" &&
        typeof selector.props.onDoubleClick === "function",
      "the selector card wires click and double-click",
    );

    if (selector !== undefined) {
      invoke(selector, "onClick");
      invoke(selector, "onDoubleClick");
    }

    check(
      selectCalls.join(",") === "2" && openCalls.join(",") === "2",
      "a click selects the track and a double-click still opens it",
    );
  }

  const recovering = gridFromState({
    ...TRACK_FIXTURE,
    pages: [
      {
        id: "tracks",
        status: "recovering",
        slots: [0, 1, 2, 3, 4, 5, 6, 7],
        activeSlot: 0,
        error: "shared plugin host exited unexpectedly",
      },
    ],
  });
  const recoveringHtml = renderToStaticMarkup(
    <GridView model={recovering} onSelectTrack={() => {}} />,
  );

  check(
    recovering.pluginPage?.status === "recovering" &&
      countOf(recoveringHtml, 'data-testid="plugin-page-status"') === 1 &&
      recoveringHtml.includes("Recovering instruments"),
    "a recovering shared page renders one page-wide banner",
  );
  check(
    countOf(
      recoveringHtml,
      'data-testid="track-status" data-status="warming"',
    ) === 7,
    "page recovery turns each loaded track's status dot amber",
  );
}
