import { useRef, useState } from "react";

export interface InlineRename {
  readonly editing: boolean;
  readonly draft: string;
  readonly start: () => void;
  readonly setDraft: (value: string) => void;
  readonly save: () => void;
  readonly cancel: () => void;
}

export function useInlineRename(
  name: string,
  onSave: (name: string) => void,
): InlineRename {
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState(name);
  const finished = useRef(false);

  const finish = (save: boolean): void => {
    if (finished.current) return;

    finished.current = true;
    const next = draft.trim();

    if (save && next.length > 0 && next !== name) onSave(next);

    setEditing(false);
  };

  return {
    editing,
    draft,
    start: () => {
      finished.current = false;
      setDraft(name);
      setEditing(true);
    },
    setDraft,
    save: () => {
      finish(true);
    },
    cancel: () => {
      finish(false);
    },
  };
}
