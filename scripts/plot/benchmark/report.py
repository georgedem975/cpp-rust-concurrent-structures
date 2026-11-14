"""Validate result quality and build numeric summaries."""

from __future__ import annotations

from pathlib import Path
from typing import Sequence

import numpy as np
import pandas as pd

from .config import LANG_STYLE, STABILITY_LIMIT
from .formatters import fmt_si, format_summary_value
from .models import Pair, Series

QualityNote = tuple[str, str]


def report_quality(aggregated: pd.DataFrame) -> list[QualityNote]:
    """Return notes about unstable measurements and unavailable CPU metrics."""
    notes: list[QualityNote] = []

    unstable = aggregated[~aggregated["stable"].astype(bool)].sort_values(
        ["suite", "language", "primitive", "threads"]
    )
    for _, row in unstable.iterrows():
        notes.append(
            (
                "unstable",
                f"{row['suite']}: {LANG_STYLE[row['language']]['label']} "
                f"{row['primitive']} / {row['scenario']} @ {int(row['threads'])} "
                f"threads — spread ×{row['thr_spread']:.0f} "
                f"({fmt_si(row['thr_min'])}..{fmt_si(row['thr_max'])} ops/s)",
            )
        )

    cpu_max = aggregated.groupby(
        ["suite", "language", "primitive", "scenario"],
        as_index=False,
    )["cpu"].max()
    for _, row in cpu_max[cpu_max["cpu"] <= 0].iterrows():
        notes.append(
            (
                "CPU not measured",
                f"{row['suite']}: {LANG_STYLE[row['language']]['label']} "
                f"{row['primitive']} / {row['scenario']} — measurement window is shorter "
                "than the CPU counter resolution; metric is unavailable",
            )
        )

    return notes


def _value_at(series: Series, thread_count: int, column: str) -> float:
    row = series.data[series.data["threads"] == thread_count]
    return float(row[column].iloc[0]) if not row.empty else np.nan


def build_summary(pairs: Sequence[Pair]) -> pd.DataFrame:
    records: list[dict] = []

    for pair in pairs:
        cpp = next((item for item in pair.series if item.language == "cpp"), None)
        rust = next((item for item in pair.series if item.language == "rust"), None)
        if cpp is None or rust is None:
            continue

        max_threads = int(max(cpp.data["threads"].max(), rust.data["threads"].max()))
        record = {
            "Pair": pair.title,
            "Scenario": pair.scenario,
            "C++ structure": cpp.primitive,
            "Rust structure": rust.primitive,
            "C++ X(1), ops/s": _value_at(cpp, 1, "thr"),
            "Rust X(1), ops/s": _value_at(rust, 1, "thr"),
            f"C++ X({max_threads}), ops/s": _value_at(cpp, max_threads, "thr"),
            f"Rust X({max_threads}), ops/s": _value_at(rust, max_threads, "thr"),
            f"C++ speedup ({max_threads})": _value_at(cpp, max_threads, "speedup"),
            f"Rust speedup ({max_threads})": _value_at(rust, max_threads, "speedup"),
            f"C++ efficiency ({max_threads})": _value_at(cpp, max_threads, "efficiency"),
            f"Rust efficiency ({max_threads})": _value_at(rust, max_threads, "efficiency"),
            f"C++ p50 ({max_threads}), ns": _value_at(cpp, max_threads, "p50"),
            f"Rust p50 ({max_threads}), ns": _value_at(rust, max_threads, "p50"),
            f"C++ p99 ({max_threads}), ns": _value_at(cpp, max_threads, "p99"),
            f"Rust p99 ({max_threads}), ns": _value_at(rust, max_threads, "p99"),
            "C++ peak RSS, MB": float(cpp.data["rss"].median()),
            "Rust peak RSS, MB": float(rust.data["rss"].median()),
        }

        invalid_points: list[str] = []
        for series in (cpp, rust):
            unstable = series.data[~series.data["stable"].astype(bool)]
            invalid_points.extend(
                f"{LANG_STYLE[series.language]['label']}@{int(thread_count)}"
                for thread_count in unstable["threads"]
            )
        record["Unstable measurements"] = ", ".join(invalid_points)

        cpp_max = record[f"C++ X({max_threads}), ops/s"]
        rust_max = record[f"Rust X({max_threads}), ops/s"]
        record[f"Rust/C++ ratio ({max_threads})"] = (
            rust_max / cpp_max
            if np.isfinite(cpp_max) and cpp_max > 0
            else np.nan
        )

        records.append(record)

    return pd.DataFrame(records)


def _markdown_table(dataframe: pd.DataFrame) -> list[str]:
    columns = list(dataframe.columns)
    rows = [
        [format_summary_value(value) for value in dataframe.loc[index, columns].tolist()]
        for index in range(len(dataframe))
    ]
    widths = [
        max(len(str(column)), *(len(row[index]) for row in rows))
        if rows
        else len(str(column))
        for index, column in enumerate(columns)
    ]

    def render_line(cells: Sequence[str]) -> str:
        return "| " + " | ".join(
            str(cell).ljust(width) for cell, width in zip(cells, widths)
        ) + " |"

    return [
        render_line(columns),
        "|" + "|".join("-" * (width + 2) for width in widths) + "|",
        *(render_line(row) for row in rows),
    ]


def write_summary(
    dataframe: pd.DataFrame,
    out_dir: Path,
    notes: Sequence[QualityNote] = (),
) -> list[Path]:
    csv_path = out_dir / "summary_table.csv"
    dataframe.to_csv(csv_path, index=False, encoding="utf-8-sig")

    markdown = [
        "# Comparison summary",
        "",
        *_markdown_table(dataframe),
        "",
        "Values are medians across repetitions. X(N) is throughput at N threads,",
        "speedup = X(N)/X(1), efficiency = X(N)/(N·X(1)).",
    ]

    if notes:
        markdown.extend(
            [
                "",
                "## Data quality notes",
                "",
                f"Unstable measurements (throughput spread across repetitions greater "
                f"than ×{STABILITY_LIMIT:.0f}) are marked with an x on plots and are not "
                "used as a speedup baseline:",
                "",
                *(f"- {text}" for kind, text in notes if kind == "unstable"),
                "",
                "Metrics that could not be measured:",
                "",
                *(f"- {text}" for kind, text in notes if kind != "unstable"),
            ]
        )

    markdown_path = out_dir / "summary_table.md"
    markdown_path.write_text("\n".join(markdown) + "\n", encoding="utf-8")
    return [csv_path, markdown_path]