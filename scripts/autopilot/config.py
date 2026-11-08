from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass
from pathlib import Path


DEFAULT_MODEL = "deepseek/deepseek-flash#max"
DEFAULT_AGENT = "build"
DEFAULT_LIST = "Instrument Hub"
DEFAULT_BASE_BRANCH = "main"


class ConfigError(RuntimeError):
    pass


@dataclass(frozen=True)
class TimeManagerConfig:
    url: str
    token: str
    list_name: str


@dataclass(frozen=True)
class AutopilotConfig:
    repo: Path
    work: Path
    model: str
    agent: str
    base_branch: str
    tm: TimeManagerConfig
    serve_url: str


def _strip_jsonc(text: str) -> str:
    result: list[str] = []
    in_string = False
    escaped = False
    i = 0

    while i < len(text):
        char = text[i]

        if in_string:
            result.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            i += 1
            continue

        if char == '"':
            in_string = True
            result.append(char)
            i += 1
            continue

        if char == "/" and i + 1 < len(text):
            next_char = text[i + 1]
            if next_char == "/":
                i += 2
                while i < len(text) and text[i] not in "\r\n":
                    i += 1
                continue
            if next_char == "*":
                i += 2
                while i + 1 < len(text) and text[i : i + 2] != "*/":
                    i += 1
                i += 2
                continue

        result.append(char)
        i += 1

    return "".join(result)


def _load_jsonc(path: Path) -> dict:
    text = _strip_jsonc(path.read_text())
    text = re.sub(r",(\s*[}\]])", r"\1", text)
    return json.loads(text)


def _opencode_config_paths() -> list[Path]:
    config_home = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    base = config_home / "opencode"
    return [base / "opencode.jsonc", base / "opencode.json"]


def _mcp_entry(config: dict) -> dict | None:
    mcp = config.get("mcp")
    if not isinstance(mcp, dict):
        return None

    entry = mcp.get("time-manager")
    if isinstance(entry, dict):
        return entry

    servers = mcp.get("servers")
    if isinstance(servers, dict) and isinstance(servers.get("time-manager"), dict):
        return servers["time-manager"]

    return None


def _token_from_entry(entry: dict) -> str | None:
    headers = entry.get("headers")
    if not isinstance(headers, dict):
        return None

    for key, value in headers.items():
        if key.lower() == "authorization" and isinstance(value, str):
            token = value.removeprefix("Bearer ").removeprefix("bearer ").strip()
            return token or None

    return None


def _time_manager_from_opencode(list_name: str) -> TimeManagerConfig:
    for path in _opencode_config_paths():
        if not path.exists():
            continue
        try:
            config = _load_jsonc(path)
        except (OSError, json.JSONDecodeError):
            continue

        entry = _mcp_entry(config)
        if entry is None:
            continue

        url = entry.get("url")
        token = _token_from_entry(entry)
        if isinstance(url, str) and token is not None:
            return TimeManagerConfig(url=url, token=token, list_name=list_name)

    raise ConfigError(
        "Time Manager MCP config was not found. Set IH_TM_MCP_URL and IH_TM_TOKEN."
    )


def _model() -> str:
    model = os.environ.get("IH_MODEL", DEFAULT_MODEL)
    if "pro" in model.lower():
        raise ConfigError(
            "IH_MODEL must not use a Pro model; use deepseek/deepseek-flash#max"
        )
    return model


def load_config(
    *,
    repo: Path,
    list_name: str,
    tm_url: str | None,
    tm_token: str | None,
) -> AutopilotConfig:
    repo = repo.resolve()
    work = repo / ".autopilot"
    url = tm_url or os.environ.get("IH_TM_MCP_URL")
    token = tm_token or os.environ.get("IH_TM_TOKEN")

    if url and token:
        tm = TimeManagerConfig(url=url, token=token, list_name=list_name)
    elif url or token:
        from_config = _time_manager_from_opencode(list_name)
        tm = TimeManagerConfig(
            url=url or from_config.url,
            token=token or from_config.token,
            list_name=list_name,
        )
    else:
        tm = _time_manager_from_opencode(list_name)

    return AutopilotConfig(
        repo=repo,
        work=work,
        model=_model(),
        agent=os.environ.get("IH_AGENT", DEFAULT_AGENT),
        base_branch=os.environ.get("IH_BASE_BRANCH", DEFAULT_BASE_BRANCH),
        tm=tm,
        serve_url=os.environ.get("IH_SERVE_URL", ""),
    )
