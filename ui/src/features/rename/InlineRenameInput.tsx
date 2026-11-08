import type { ReactElement } from "react";

export const INLINE_NAME_MAX_LENGTH = 80;

export interface InlineRenameInputProps {
  readonly value: string;
  readonly ariaLabel: string;
  readonly testId: string;
  readonly className: string;
  readonly inputClassName: string;
  readonly maxLength?: number;
  readonly onValueChange: (value: string) => void;
  readonly onSave: () => void;
  readonly onCancel: () => void;
}

export function InlineRenameInput({
  value,
  ariaLabel,
  testId,
  className,
  inputClassName,
  maxLength = INLINE_NAME_MAX_LENGTH,
  onValueChange,
  onSave,
  onCancel,
}: InlineRenameInputProps): ReactElement {
  return (
    <form
      className={className}
      onSubmit={(event) => {
        event.preventDefault();
        onSave();
      }}
      onKeyDown={(event) => {
        if (event.key !== "Escape") return;

        event.preventDefault();
        onCancel();
      }}
    >
      <input
        className={inputClassName}
        data-testid={testId}
        value={value}
        maxLength={maxLength}
        autoFocus
        aria-label={ariaLabel}
        onChange={(event) => {
          onValueChange(event.target.value);
        }}
        onBlur={onSave}
      />
    </form>
  );
}
