import type { ReactElement } from "react";

import type { ControlAudioPatch, ControlAudioState } from "../../api/client";
import {
  forgetDevicePatch,
  restoreFallback,
  storedDeviceLabel,
  storedDevicePatch,
} from "./model";

interface RestoreNoticeProps {
  readonly audio: ControlAudioState;
  readonly busy: boolean;
  readonly onChange?: ((patch: ControlAudioPatch) => void) | undefined;
}

export function RestoreNotice({
  audio,
  busy,
  onChange,
}: RestoreNoticeProps): ReactElement | null {
  const notice = restoreFallback(audio);

  if (notice === null) return null;

  const stored = storedDevicePatch(audio);
  const disabled = busy || onChange === undefined;

  return (
    <aside
      className="audio-settings__restore"
      data-testid="audio-restore"
      data-fallback={String(audio.restore.fallback)}
    >
      <div className="audio-settings__restore-copy">
        <p className="audio-settings__restore-eyebrow">Using fallback</p>
        <p
          className="audio-settings__restore-device"
          data-testid="audio-restore-device"
        >
          {notice.device}
        </p>
        <p
          className="audio-settings__restore-reason"
          data-testid="audio-restore-reason"
        >
          {notice.reason}
        </p>
        {stored === null ? null : (
          <p
            className="audio-settings__restore-stored"
            data-testid="audio-restore-stored"
          >
            Stored: {storedDeviceLabel(stored)}
          </p>
        )}
      </div>

      <div className="audio-settings__restore-actions">
        <button
          type="button"
          data-testid="audio-use-stored-device"
          disabled={disabled || stored === null}
          onClick={() => {
            if (stored !== null) onChange?.(stored);
          }}
        >
          Use this device
        </button>
        <button
          type="button"
          data-testid="audio-forget-device"
          disabled={disabled}
          onClick={() => {
            onChange?.(forgetDevicePatch(audio));
          }}
        >
          Forget device
        </button>
      </div>
    </aside>
  );
}
