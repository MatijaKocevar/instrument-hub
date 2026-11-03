export function sectionOf(html: string, start: string, end: string): string {
  const from = html.indexOf(start);

  if (from < 0) return "";

  const to = html.indexOf(end, from + start.length);

  return to < 0 ? "" : html.slice(from, to + end.length);
}
