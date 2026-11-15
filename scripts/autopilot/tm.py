from __future__ import annotations

import json
import re
import time
import urllib.error
import urllib.request
from datetime import datetime
from typing import Any

from .config import TimeManagerConfig
from .log import log


class TimeManagerError(RuntimeError):
    pass


def _task_number(task: dict[str, Any]) -> int | None:
    match = re.match(r"^\s*T(\d+)\b", str(task.get("title", "")))
    return int(match.group(1)) if match else None


def sort_tasks(tasks: list[dict[str, Any]]) -> list[dict[str, Any]]:
    def key(task: dict[str, Any]) -> tuple[int, int, str]:
        number = _task_number(task)
        if number is None:
            return (1, 0, str(task.get("id", "")))
        return (0, number, str(task.get("id", "")))

    return sorted(tasks, key=key)


class TimeManagerClient:
    def __init__(self, config: TimeManagerConfig, *, timeout: float = 30.0) -> None:
        self.config = config
        self.timeout = timeout
        self._request_id = 0
        self._initialized = False

    def _post(self, payload: dict[str, Any]) -> dict[str, Any]:
        body = json.dumps(payload).encode()
        request = urllib.request.Request(
            self.config.url,
            data=body,
            headers={
                "Authorization": f"Bearer {self.config.token}",
                "Content-Type": "application/json",
                "Accept": "application/json, text/event-stream",
            },
            method="POST",
        )

        with urllib.request.urlopen(request, timeout=self.timeout) as response:
            raw = response.read().decode()
            content_type = response.headers.get("Content-Type", "")

        if "text/event-stream" in content_type:
            return self._parse_sse(raw, payload.get("id"))

        return self._parse_json(raw, payload.get("id"))

    def _parse_json(self, raw: str, request_id: Any) -> dict[str, Any]:
        try:
            parsed = json.loads(raw)
        except json.JSONDecodeError as error:
            raise TimeManagerError(f"invalid JSON response: {error}") from error

        if not isinstance(parsed, dict):
            raise TimeManagerError("invalid JSON-RPC response")
        return parsed

    def _parse_sse(self, raw: str, request_id: Any) -> dict[str, Any]:
        events: list[str] = []
        current: list[str] = []

        for line in raw.splitlines():
            if line.startswith("data:"):
                current.append(line[5:].lstrip())
            elif not line.strip() and current:
                events.append("\n".join(current))
                current = []

        if current:
            events.append("\n".join(current))

        for event in events:
            try:
                parsed = json.loads(event)
            except json.JSONDecodeError:
                continue

            if not isinstance(parsed, dict):
                continue
            if request_id is None or parsed.get("id") == request_id:
                return parsed

        if events:
            try:
                parsed = json.loads(events[-1])
            except json.JSONDecodeError:
                parsed = None
            if isinstance(parsed, dict):
                return parsed

        raise TimeManagerError("no JSON-RPC response in SSE stream")

    def _rpc(
        self,
        method: str,
        params: dict[str, Any] | None = None,
        *,
        attempts: int = 6,
    ) -> dict[str, Any]:
        self._request_id += 1
        request_id = self._request_id
        payload = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
            "params": params or {},
        }

        last_error: Exception | None = None
        for attempt in range(1, attempts + 1):
            try:
                response = self._post(payload)
                if "error" in response:
                    error = response["error"]
                    message = (
                        error.get("message", str(error))
                        if isinstance(error, dict)
                        else str(error)
                    )
                    raise TimeManagerError(f"{method}: {message}")
                return response
            except urllib.error.HTTPError as error:
                last_error = error
                if error.code not in (429, 500, 502, 503, 504):
                    raise TimeManagerError(f"{method}: HTTP {error.code}") from error
            except (urllib.error.URLError, TimeoutError, TimeManagerError) as error:
                last_error = error

            if attempt < attempts:
                delay = min(30, 2**attempt)
                log(f"TM retry {method} in {delay}s: {last_error}")
                time.sleep(delay)

        raise TimeManagerError(f"{method}: {last_error}")

    def initialize(self) -> None:
        if self._initialized:
            return

        self._rpc(
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "instrument-hub-autopilot", "version": "1"},
            },
        )
        self._initialized = True

    def call_tool(self, name: str, arguments: dict[str, Any]) -> Any:
        self.initialize()
        response = self._rpc("tools/call", {"name": name, "arguments": arguments})
        result = response.get("result")
        if not isinstance(result, dict):
            raise TimeManagerError(f"{name}: invalid tool result")

        if result.get("isError") is True:
            raise TimeManagerError(f"{name}: {self._result_text(result)}")

        text = self._result_text(result)
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return text

    @staticmethod
    def _result_text(result: dict[str, Any]) -> str:
        content = result.get("content")
        if not isinstance(content, list):
            return ""
        parts = [
            str(item.get("text", ""))
            for item in content
            if isinstance(item, dict) and item.get("type") == "text"
        ]
        return "\n".join(parts)

    def resolve_list_id(self) -> str:
        lists = self.call_tool("list_task_lists", {})
        if not isinstance(lists, list):
            raise TimeManagerError("list_task_lists did not return a list")

        wanted = self.config.list_name.casefold()
        for task_list in lists:
            if not isinstance(task_list, dict):
                continue
            if str(task_list.get("name", "")).casefold() == wanted:
                return str(task_list["id"])

        names = ", ".join(
            str(item.get("name")) for item in lists if isinstance(item, dict)
        )
        raise TimeManagerError(
            f"task list {self.config.list_name!r} not found (available: {names})"
        )

    def list_tasks(self, list_id: str, status: str = "TODO") -> list[dict[str, Any]]:
        tasks = self.call_tool("list_tasks", {"status": status, "listId": list_id})
        if not isinstance(tasks, list):
            raise TimeManagerError("list_tasks did not return a list")
        return [task for task in tasks if isinstance(task, dict)]

    def get_task(self, task_id: str) -> dict[str, Any]:
        task = self.call_tool("get_task", {"id": task_id})
        if not isinstance(task, dict):
            raise TimeManagerError(f"get_task returned no task for {task_id}")
        return task

    def update_task(
        self,
        task_id: str,
        *,
        status: str | None = None,
        description: str | None = None,
    ) -> None:
        arguments: dict[str, Any] = {"id": task_id}
        if status is not None:
            arguments["status"] = status
        if description is not None:
            arguments["description"] = description

        result = self.call_tool("update_task", arguments)
        if isinstance(result, dict) and result.get("success") is False:
            raise TimeManagerError(f"update_task failed for {task_id}")


def task_label(task: dict[str, Any]) -> str:
    return str(task.get("title") or task.get("id") or "untitled")


def commit_subject(task: dict[str, Any]) -> str:
    label = task_label(task)
    text = label.lower()

    if re.search(r"\b(bug|fix|crash|hang|regression)\b", text):
        kind = "fix"
    elif re.search(r"\b(tests?|snapshot|acceptance|harness)\b", text):
        kind = "test"
    elif re.search(r"\b(split|decompose|extract|refactor|sweep)\b", text):
        kind = "refactor"
    elif re.search(r"\b(architecture|handover|readme|docs?)\b", text):
        kind = "docs"
    elif re.search(r"\b(ci|workflow)\b", text):
        kind = "ci"
    elif re.search(r"\b(packaging|appimage)\b", text):
        kind = "build"
    elif re.search(r"\b(perf|performance|optimize|optimise|faster|speed)\b", text):
        kind = "perf"
    elif re.search(r"\b(bootstrap|prettier|ledger|config|policy|tooling)\b", text):
        kind = "chore"
    else:
        kind = "feat"

    return f"{kind}: {label}"


def task_branch_slug(task: dict[str, Any]) -> str:
    title = str(task.get("title", ""))
    number = _task_number(task)
    if number is not None:
        title = re.sub(r"^\s*T\d+\s*[·:-]?\s*", "", title)
        prefix = f"T{number}"
    else:
        prefix = str(task.get("id", "task"))[-8:]

    slug = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
    words: list[str] = []
    for word in slug.split("-"):
        candidate = "-".join([*words, word])
        if len(candidate) > 48:
            break
        words.append(word)
    slug = "-".join(words) or slug[:48].rstrip("-") or "task"
    return f"task/{prefix}-{slug}"


def task_sort_label(task: dict[str, Any]) -> str:
    number = _task_number(task)
    return f"T{number}" if number is not None else str(task.get("id", "?"))[-8:]


def task_created_at(task: dict[str, Any]) -> datetime | None:
    value = task.get("createdAt") or task.get("updatedAt")
    if not isinstance(value, str):
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
