"""Number formatters for plot labels and summary tables."""

from __future__ import annotations

import numpy as np


def fmt_si(value: float, _position: int | None = None) -> str:
    if value <= 0:
        return "0"
    for divisor, suffix in ((1e9, "G"), (1e6, "M"), (1e3, "K")):
        if value >= divisor:
            scaled = value / divisor
            return f"{scaled:.0f}{suffix}" if scaled >= 10 else f"{scaled:.1f}{suffix}"
    return f"{value:.0f}"


def fmt_ns(value: float, _position: int | None = None) -> str:
    if value <= 0:
        return "0"
    for divisor, suffix in ((1e6, " ms"), (1e3, " µs"), (1, " ns")):
        if value >= divisor:
            scaled = value / divisor
            return f"{scaled:.0f}{suffix}" if scaled >= 10 else f"{scaled:.1f}{suffix}"
    return f"{value:.1f} ns"


def fmt_mb(value: float, _position: int | None = None) -> str:
    if value <= 0:
        return "0"
    if value >= 1024:
        return f"{value / 1024:.1f} GB"
    return f"{value:.0f} MB"


def fmt_integer(value: float, _position: int | None = None) -> str:
    return f"{value:.0f}"


def fmt_percent(value: float, _position: int | None = None) -> str:
    return f"{value * 100:.0f}%"


def fmt_speedup_cell(value: float) -> str:
    if value >= 10:
        return f"{value:.0f}×"
    if value >= 1:
        return f"{value:.1f}×"
    return f"{value:.2f}×"


def fmt_pct_cell(value: float) -> str:
    return f"{value * 100:.0f}%"


def format_summary_value(value: object) -> str:
    """Convert a DataFrame scalar to a value for a Markdown table."""
    if isinstance(value, (int, np.integer)):
        return str(int(value))
    if isinstance(value, (float, np.floating)):
        if not np.isfinite(value):
            return "—"
        if abs(value) >= 1000:
            return f"{value:,.0f}".replace(",", " ")
        return f"{value:.2f}"
    return str(value)