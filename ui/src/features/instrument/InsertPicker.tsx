import {
  useEffect,
  useRef,
  useState,
  type CSSProperties,
  type KeyboardEvent,
  type ReactElement,
} from "react";

import "./insert-picker.css";

export interface InsertPickerOption {
  readonly value: string;
  readonly label: string;
}

export interface InsertPickerGroup {
  readonly label: string;
  readonly options: readonly InsertPickerOption[];
}

interface InsertPickerProps {
  readonly id: string;
  readonly value: string;
  readonly groups: readonly InsertPickerGroup[];
  readonly testId?: string;
  readonly labelledBy?: string;
  readonly onChange: (value: string) => void;
}

const menuMargin = 8;
const menuGap = 4;
const minimumMenuHeight = 140;

function menuPosition(trigger: HTMLButtonElement | null): CSSProperties {
  if (trigger === null) return {};

  const rect = trigger.getBoundingClientRect();
  const width = Math.max(rect.width, 220);
  const left = Math.min(
    Math.max(menuMargin, rect.left),
    Math.max(menuMargin, window.innerWidth - width - menuMargin),
  );
  const spaceBelow = window.innerHeight - rect.bottom - menuGap - menuMargin;
  const spaceAbove = rect.top - menuGap - menuMargin;
  const openAbove = spaceBelow < minimumMenuHeight && spaceAbove > spaceBelow;
  const available = openAbove ? spaceAbove : spaceBelow;
  const maxHeight = Math.max(80, Math.min(360, available));

  return openAbove
    ? {
        left,
        width,
        bottom: window.innerHeight - rect.top + menuGap,
        maxHeight,
      }
    : { left, width, top: rect.bottom + menuGap, maxHeight };
}

export function InsertPicker({
  id,
  value,
  groups,
  testId,
  labelledBy,
  onChange,
}: InsertPickerProps): ReactElement {
  const [open, setOpen] = useState(false);
  const [position, setPosition] = useState<CSSProperties>({});
  const trigger = useRef<HTMLButtonElement>(null);
  const menu = useRef<HTMLDivElement>(null);
  const menuId = `${id}-menu`;
  const valueId = `${id}-value`;
  const options = groups.flatMap((group) => group.options);
  const selected = options.find((option) => option.value === value);

  const openMenu = (): void => {
    setPosition(menuPosition(trigger.current));
    setOpen(true);
  };

  const closeMenu = (returnFocus: boolean): void => {
    setOpen(false);

    if (returnFocus) trigger.current?.focus();
  };

  useEffect(() => {
    if (!open) return;

    const dismiss = (event: PointerEvent): void => {
      const target = event.target as Node | null;

      if (target === null) return;
      if (trigger.current?.contains(target) === true) return;
      if (menu.current?.contains(target) === true) return;

      setOpen(false);
    };

    const relocate = (): void => {
      setPosition(menuPosition(trigger.current));
    };

    const escape = (event: globalThis.KeyboardEvent): void => {
      if (event.key === "Escape") closeMenu(true);
    };

    document.addEventListener("pointerdown", dismiss);
    document.addEventListener("keydown", escape);
    window.addEventListener("resize", relocate);
    window.addEventListener("scroll", relocate, true);

    return () => {
      document.removeEventListener("pointerdown", dismiss);
      document.removeEventListener("keydown", escape);
      window.removeEventListener("resize", relocate);
      window.removeEventListener("scroll", relocate, true);
    };
  }, [open]);

  useEffect(() => {
    if (!open) return;

    const current =
      menu.current?.querySelector<HTMLButtonElement>(
        '[role="option"][aria-selected="true"]',
      ) ?? menu.current?.querySelector<HTMLButtonElement>('[role="option"]');

    current?.focus();
  }, [open]);

  const moveFocus = (delta: number): void => {
    const elements = Array.from(
      menu.current?.querySelectorAll<HTMLButtonElement>('[role="option"]') ??
        [],
    );

    if (elements.length === 0) return;

    const current = elements.indexOf(
      document.activeElement as HTMLButtonElement,
    );
    const next =
      elements[(current + delta + elements.length) % elements.length];

    next?.focus();
  };

  const onMenuKeyDown = (event: KeyboardEvent<HTMLDivElement>): void => {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      moveFocus(1);
      return;
    }

    if (event.key === "ArrowUp") {
      event.preventDefault();
      moveFocus(-1);
      return;
    }

    if (event.key === "Home") {
      event.preventDefault();
      menu.current
        ?.querySelector<HTMLButtonElement>('[role="option"]')
        ?.focus();
      return;
    }

    if (event.key === "End") {
      event.preventDefault();
      const elements =
        menu.current?.querySelectorAll<HTMLButtonElement>('[role="option"]');
      elements?.[elements.length - 1]?.focus();
      return;
    }

    if (event.key === "Escape") {
      event.preventDefault();
      closeMenu(true);
    }
  };

  return (
    <div className="instrument__insert-picker">
      <button
        ref={trigger}
        id={id}
        className="instrument__select instrument__picker-trigger"
        type="button"
        aria-haspopup="listbox"
        aria-expanded={open}
        aria-controls={open ? menuId : undefined}
        aria-labelledby={
          labelledBy === undefined ? undefined : `${labelledBy} ${valueId}`
        }
        data-testid={testId}
        onClick={() => {
          if (open) {
            closeMenu(false);
          } else {
            openMenu();
          }
        }}
        onKeyDown={(event) => {
          if (event.key === "ArrowDown" || event.key === "ArrowUp") {
            event.preventDefault();
            openMenu();
          }
        }}
      >
        <span id={valueId} className="instrument__picker-trigger-label">
          {selected?.label ?? "Choose…"}
        </span>
        <span className="instrument__picker-chevron" aria-hidden="true">
          ▾
        </span>
      </button>

      {open ? (
        <div
          ref={menu}
          id={menuId}
          className="instrument__picker-menu"
          role="listbox"
          aria-label="Insert options"
          style={position}
          onKeyDown={onMenuKeyDown}
          onBlur={(event) => {
            const next = event.relatedTarget as Node | null;

            if (next === null || menu.current?.contains(next) !== true)
              setOpen(false);
          }}
        >
          {groups.map((group) => (
            <div
              key={group.label}
              className="instrument__picker-group"
              role="group"
              aria-label={group.label}
            >
              <div className="instrument__picker-group-label">
                {group.label}
              </div>
              {group.options.map((option) => {
                const isSelected = option.value === value;

                return (
                  <button
                    key={option.value}
                    className="instrument__picker-option"
                    type="button"
                    role="option"
                    aria-selected={isSelected}
                    data-testid="fx-add-option"
                    data-value={option.value}
                    data-selected={isSelected ? "true" : "false"}
                    onClick={() => {
                      onChange(option.value);
                      closeMenu(true);
                    }}
                  >
                    {option.label}
                  </button>
                );
              })}
            </div>
          ))}
        </div>
      ) : null}
    </div>
  );
}
