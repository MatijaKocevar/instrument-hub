import type { ConnectionPhase } from "../../state/store";
import type { ShellRoute } from "../../state/router";

export interface NavItem {
  readonly route: ShellRoute;
  readonly label: string;
  readonly shortLabel: string;
}

export const WORKSPACE_ITEMS: readonly NavItem[] = [
  { route: "grid", label: "Grid", shortLabel: "Grid" },
  { route: "instrument", label: "Instrument", shortLabel: "Instrument" },
  { route: "plugins", label: "Plugins", shortLabel: "Plugins" },
];

export const AUDIO_ITEM: NavItem = {
  route: "audio",
  label: "Audio & MIDI",
  shortLabel: "Audio",
};

export const SETTINGS_ITEM: NavItem = {
  route: "settings",
  label: "Settings & Account",
  shortLabel: "Settings",
};

export interface ShellNavProps {
  readonly route: ShellRoute;
  readonly navigate: (path: string) => void;
  readonly phase: ConnectionPhase;
  readonly lanEnabled: boolean;
  readonly paired: boolean;
  readonly version: string;
}
