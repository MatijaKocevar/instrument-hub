from __future__ import annotations

import time
from datetime import datetime, timedelta, timezone

from .log import log


PEAK_BLOCKS = ((1, 4), (6, 10))
DEFAULT_MARGIN_MINUTES = 20


def _as_utc(value: datetime) -> datetime:
    if value.tzinfo is None:
        return value.replace(tzinfo=timezone.utc)
    return value.astimezone(timezone.utc)


def is_peak(value: datetime) -> bool:
    value = _as_utc(value)
    if value.weekday() >= 5:
        return False
    return any(start <= value.hour < end for start, end in PEAK_BLOCKS)


def _peak_end(value: datetime) -> datetime:
    value = _as_utc(value)
    for start, end in PEAK_BLOCKS:
        if start <= value.hour < end:
            return value.replace(hour=end, minute=0, second=0, microsecond=0)
    return value


def _too_close_to_peak(value: datetime, margin: timedelta) -> bool:
    value = _as_utc(value)
    if value.weekday() >= 5:
        return False
    for start, _ in PEAK_BLOCKS:
        boundary = value.replace(hour=start, minute=0, second=0, microsecond=0)
        if timedelta(0) <= boundary - value <= margin:
            return True
    return False


def next_safe_start(
    value: datetime,
    margin_minutes: int = DEFAULT_MARGIN_MINUTES,
) -> datetime:
    value = _as_utc(value).replace(second=0, microsecond=0)
    margin = timedelta(minutes=max(0, margin_minutes))

    if is_peak(value):
        return _peak_end(value)

    if _too_close_to_peak(value, margin):
        for start, end in PEAK_BLOCKS:
            boundary = value.replace(hour=start, minute=0, second=0, microsecond=0)
            if timedelta(0) <= boundary - value <= margin:
                return value.replace(hour=end, minute=0, second=0, microsecond=0)

    return value


def human_delta(seconds: float) -> str:
    minutes = max(0, int(seconds // 60))
    if minutes >= 60:
        return f"{minutes // 60}h{minutes % 60:02d}m"
    return f"{minutes}m"


def wait_for_offpeak(
    *,
    ignore: bool,
    margin_minutes: int = DEFAULT_MARGIN_MINUTES,
) -> None:
    if ignore:
        return

    now = datetime.now(timezone.utc)
    target = next_safe_start(now, margin_minutes)
    if target <= now:
        return

    seconds = (target - now).total_seconds()
    log(f"WAIT peak pricing until {target:%Y-%m-%d %H:%M}Z ({human_delta(seconds)})")

    while True:
        remaining = (target - datetime.now(timezone.utc)).total_seconds()
        if remaining <= 0:
            break
        time.sleep(min(remaining, 300))

    log("GO off-peak")
