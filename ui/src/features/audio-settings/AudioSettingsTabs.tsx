import type { ReactElement } from "react";

export type AudioSettingsTab = "device" | "channels" | "midi";

const tabs: ReadonlyArray<{ id: AudioSettingsTab; label: string }> = [
  { id: "device", label: "Device" },
  { id: "channels", label: "Channels" },
  { id: "midi", label: "MIDI" },
];

interface AudioSettingsTabsProps {
  readonly tab: AudioSettingsTab;
  readonly onChange: (tab: AudioSettingsTab) => void;
}

export function AudioSettingsTabs({
  tab,
  onChange,
}: AudioSettingsTabsProps): ReactElement {
  return (
    <div
      className="audio-settings__tabs"
      role="tablist"
      aria-label="Audio settings sections"
      data-testid="audio-settings-tabs"
    >
      {tabs.map((entry) => (
        <button
          key={entry.id}
          type="button"
          role="tab"
          id={`audio-tab-${entry.id}`}
          aria-selected={tab === entry.id}
          aria-controls={`audio-panel-${entry.id}`}
          className="audio-settings__tab"
          data-testid={`audio-tab-${entry.id}`}
          onClick={() => {
            onChange(entry.id);
          }}
        >
          {entry.label}
        </button>
      ))}
    </div>
  );
}
