"""Data models used throughout the plotting pipeline."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import pandas as pd

from .config import LANG_STYLE


@dataclass
class Series:
    """Results for one data structure, language, and scenario."""

    language: str
    primitive: str
    scenario: str
    data: pd.DataFrame
    baseline_ok: bool = True

    @property
    def label(self) -> str:
        language_label = LANG_STYLE[self.language]["label"]
        return f"{language_label} {self.primitive}"

    @property
    def title(self) -> str:
        language_label = LANG_STYLE[self.language]["label"]
        return f"{language_label}: {self.primitive} / {self.scenario}"

    @property
    def short(self) -> str:
        return f"{LANG_STYLE[self.language]['label']} {self.primitive}"


@dataclass
class Pair:
    """Matched C++ and Rust series for one scenario."""

    title: str
    scenario: str
    series: list[Series]

    @property
    def slug(self) -> str:
        primitives = {item.primitive for item in self.series}
        if len(primitives) == 1:
            base = next(iter(primitives))
        else:
            base = "-".join(item.primitive for item in self.series)
        return slugify(f"{base}_{self.scenario}")


@dataclass
class Context:
    """Plotting settings for one benchmark suite."""

    out_dir: Path
    threads: list[int]
    physical: int
    logical: int
    dpi: int
    formats: list[str]
    suite_meta: dict


@dataclass
class PanelSpec:
    """Description of one detailed plot panel."""

    key: str
    title: str
    ylabel: str
    logy: bool = False
    yfmt: Callable[[float, int], str] | None = None
    ideal: str | None = None


def slugify(value: str) -> str:
    """Convert a name into a safe filename component."""
    value = value.replace("↔", "-").replace("/", "-")
    return re.sub(r"[^0-9A-Za-z_.+-]+", "_", value).strip("_")