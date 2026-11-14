import { useState, type ReactElement } from "react";

import type {
  ControlFxInsertRequest,
  ControlPatch,
  ControlPlugin,
} from "../../api/client";
import { DRIVE_PRESETS, insertRequestForDrivePreset } from "./drive";
import {
  BUILTIN_INSERT_TYPES,
  insertLabel,
  insertRequestForBuiltin,
  insertRequestForPlugin,
  type BuiltinInsertType,
} from "./inserts";
import { InsertPicker, type InsertPickerGroup } from "./InsertPicker";
import { effectPlugins } from "./selection";
import { insertRequestForTuner } from "./tuner";

interface InsertAddProps {
  readonly patch: ControlPatch;
  readonly plugins?: readonly ControlPlugin[] | null;
  readonly existingIds?: readonly string[];
  readonly chainBusy?: boolean;
  readonly onAddInsert?: (slot: number, insert: ControlFxInsertRequest) => void;
}

export function InsertAdd({
  patch,
  plugins,
  existingIds = [],
  chainBusy,
  onAddInsert,
}: InsertAddProps): ReactElement {
  const [selection, setSelection] = useState<string>(
    `drive:${DRIVE_PRESETS[0].id}`,
  );
  const effects = effectPlugins(plugins ?? []);
  const groups: InsertPickerGroup[] = [
    {
      label: "Drive",
      options: DRIVE_PRESETS.map((preset) => ({
        value: `drive:${preset.id}`,
        label: preset.label,
      })),
    },
    {
      label: "Built-in",
      options: BUILTIN_INSERT_TYPES.map((type) => ({
        value: `builtin:${type}`,
        label: insertLabel(type),
      })),
    },
    {
      label: "Tools",
      options: [{ value: "tuner", label: "Tuner" }],
    },
  ];

  if (effects.length > 0)
    groups.push({
      label: "Plug-ins",
      options: effects.map((plugin) => ({
        value: `plugin:${plugin.id}`,
        label: `${plugin.name} · ${plugin.vendor}`,
      })),
    });

  const add = (): void => {
    if (selection === "tuner") {
      onAddInsert?.(patch.slot, insertRequestForTuner(existingIds));
      return;
    }

    if (selection.startsWith("drive:")) {
      const presetId = selection.slice("drive:".length);
      const preset = DRIVE_PRESETS.find(
        (candidate) => candidate.id === presetId,
      );

      if (preset !== undefined)
        onAddInsert?.(
          patch.slot,
          insertRequestForDrivePreset(preset, existingIds),
        );

      return;
    }

    if (selection.startsWith("builtin:")) {
      const type = selection.slice("builtin:".length) as BuiltinInsertType;

      if ((BUILTIN_INSERT_TYPES as readonly string[]).includes(type))
        onAddInsert?.(patch.slot, insertRequestForBuiltin(type));

      return;
    }

    if (selection.startsWith("plugin:")) {
      const plugin = effects.find(
        (candidate) => candidate.id === selection.slice("plugin:".length),
      );

      if (plugin !== undefined)
        onAddInsert?.(patch.slot, insertRequestForPlugin(plugin));
    }
  };

  return (
    <div className="instrument__add" data-testid="fx-add">
      <span className="instrument__add-title">Load insert</span>
      <div className="instrument__add-field">
        <span
          className="instrument__field-label"
          id="instrument-insert-select-label"
        >
          Insert
        </span>
        <InsertPicker
          id="instrument-insert-select"
          value={selection}
          groups={groups}
          testId="fx-add-select"
          labelledBy="instrument-insert-select-label"
          onChange={setSelection}
        />
      </div>

      <button
        className="instrument__button"
        type="button"
        data-testid="fx-add-button"
        disabled={chainBusy === true || onAddInsert === undefined}
        onClick={add}
      >
        + Add insert
      </button>
    </div>
  );
}
