from __future__ import annotations

import html
import re
from html.parser import HTMLParser


MARKER = "<hr>\n<p>Autopilot run:"


class _TextExtractor(HTMLParser):
    block_tags = {
        "p",
        "div",
        "section",
        "article",
        "header",
        "footer",
        "h1",
        "h2",
        "h3",
        "h4",
        "h5",
        "h6",
        "ul",
        "ol",
        "li",
        "br",
        "hr",
        "tr",
    }

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.parts: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag in self.block_tags:
            self.parts.append("\n")
        if tag == "li":
            self.parts.append("- ")

    def handle_endtag(self, tag: str) -> None:
        if tag in self.block_tags:
            self.parts.append("\n")

    def handle_data(self, data: str) -> None:
        self.parts.append(data)

    def text(self) -> str:
        raw = "".join(self.parts)
        raw = re.sub(r"[ \t]+", " ", raw)
        raw = re.sub(r" *\n *", "\n", raw)
        raw = re.sub(r"\n{3,}", "\n\n", raw)
        return raw.strip()


def html_to_text(value: str) -> str:
    parser = _TextExtractor()
    parser.feed(value)
    return parser.text()


def _item(label: str, value: str) -> str:
    return f"<li>{html.escape(label)}: {html.escape(value)}</li>"


def build_comment(
    *,
    outcome: str,
    task_title: str,
    branch: str,
    summary: str = "",
    verification: str = "",
    reason: str = "",
    needed: str = "",
    commit: str = "",
) -> str:
    lines = [f"<p>Autopilot run: {html.escape(outcome)}</p>", "<ul>"]
    lines.append(_item("Task", task_title))
    lines.append(_item("Branch", branch))

    if commit:
        lines.append(_item("Commit", commit))
    if summary:
        lines.append(_item("Summary", summary))
    if verification:
        lines.append(_item("Verification", verification))
    if reason:
        lines.append(_item("Reason", reason))
    if needed:
        lines.append(_item("Needed", needed))

    lines.append("</ul>")
    return "\n".join(lines)


def with_comment(description: str, comment: str) -> str:
    base = description.rstrip()
    marker_at = base.rfind(MARKER)
    if marker_at >= 0:
        base = base[:marker_at].rstrip()

    if base:
        return f"{base}\n\n<hr>\n{comment}"

    return comment
