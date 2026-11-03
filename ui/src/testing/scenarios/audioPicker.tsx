import { renderToStaticMarkup } from "react-dom/server";

import {
  DevicePicker,
  deviceDescription,
  pickerDevices,
  rawDevices,
  unavailableReason,
} from "../../features/audio-settings";
import { AUDIO } from "../fixtures";
import { check, countOf } from "../harness";

export function audioPickerModelTest(): void {
  const devices = pickerDevices(AUDIO);

  check(AUDIO.devices.length === 5, "the fixture carries a duplicate card row");
  check(devices.length === 4, "the picker deduplicates the card rows");
  check(
    devices.filter((device) => device.name === "Steinberg UR22mkII").length ===
      1,
    "the duplicate card appears once",
  );

  const unavailable = devices.find((device) => device.available === false);

  check(
    unavailable?.name === "HD-Audio Generic (HDMI)",
    "the unavailable card is kept in the list",
  );
  check(
    unavailable !== undefined &&
      unavailableReason(unavailable) === "in use by another process",
    "the unavailable card keeps its reason",
  );
  check(
    devices[0] !== undefined &&
      deviceDescription(devices[0]) === "USB audio interface",
    "a card exposes its short description",
  );
  check(rawDevices(AUDIO).length === 5, "the raw PCM list is available");
}

export function audioPickerRenderTest(): void {
  const closed = renderToStaticMarkup(
    <DevicePicker audio={AUDIO} busy={false} onChange={() => {}} />,
  );

  check(
    countOf(closed, 'data-testid="device-card"') === 4,
    "the closed picker renders four device cards",
  );
  check(
    closed.includes('data-testid="device-reason">in use by another process'),
    "the unavailable card renders its reason",
  );
  check(
    closed.includes('data-available="false" data-selected="false" disabled=""'),
    "the unavailable card renders disabled",
  );
  check(
    closed.includes('data-testid="device-advanced" data-open="false" hidden'),
    "the raw PCM list starts hidden",
  );

  const open = renderToStaticMarkup(
    <DevicePicker
      audio={AUDIO}
      busy={false}
      onChange={() => {}}
      initialAdvanced
    />,
  );

  check(
    open.includes('data-testid="device-advanced" data-open="true"'),
    "the Advanced toggle opens the raw PCM list",
  );
  check(
    !open.includes('data-testid="device-advanced" data-open="true" hidden'),
    "the open raw PCM list is not hidden",
  );
  check(
    open.includes("hw:CARD=UR22mkII,DEV=0"),
    "the raw PCM list shows the underlying PCM names",
  );
  check(
    open.includes('data-testid="device-select"'),
    "the raw PCM list offers a selection control",
  );
}
