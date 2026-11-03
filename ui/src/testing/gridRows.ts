export interface RenderedGridRow {
  readonly family: string;
  readonly row: number;
  readonly slots: readonly number[];
}

const ROW_MARKER = 'data-testid="family" ';
const CELL_SLOT = /data-testid="grid-cell" data-slot="(-?\d+)"/g;

export function renderedGridRows(html: string): readonly RenderedGridRow[] {
  return html
    .split(ROW_MARKER)
    .slice(1)
    .map((markup) => ({
      family: /data-family="([^"]+)"/.exec(markup)?.[1] ?? "",
      row: Number(/data-row="(\d+)"/.exec(markup)?.[1] ?? -1),
      slots: Array.from(markup.matchAll(CELL_SLOT)).flatMap((match) => {
        const slot = match[1];
        return slot === undefined ? [] : [Number(slot)];
      }),
    }));
}
