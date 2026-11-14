"""Load and aggregate results, and match C++/Rust data structures."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Sequence

import numpy as np
import pandas as pd

from .config import LIBRARY_PAIRS, STABILITY_LIMIT
from .models import Pair, Series

CPU_PERCENT = 100.0

REQUIRED_COLUMNS = {
    "language",
    "primitive",
    "scenario",
    "threads",
    "throughput_ops_s",
    "lat_p50_ns",
    "lat_p95_ns",
    "lat_p99_ns",
    "lat_p99_9_ns",
    "cpu_percent",
    "rss_peak_mb",
    "ops",
    "batch",
    "repeat"
}


def load_csv(results_dir: Path, suites: Sequence[str]) -> tuple[pd.DataFrame, dict]:
    """Read all.csv and meta.json for the specified benchmark suites."""
    frames: list[pd.DataFrame] = []
    metadata: dict[str, dict] = {}

    for suite in suites:
        csv_path = results_dir / suite / "all.csv"
        if not csv_path.is_file():
            print(f"[!] Skipping: file not found: {csv_path}")
            continue

        frame = pd.read_csv(csv_path)
        missing_columns = REQUIRED_COLUMNS - set(frame.columns)
        if missing_columns:
            missing = ", ".join(sorted(missing_columns))
            raise ValueError(f"Required columns are missing from {csv_path}: {missing}")

        frame["suite"] = suite
        frames.append(frame)

        meta_path = results_dir / suite / "meta.json"
        if meta_path.is_file():
            try:
                metadata[suite] = json.loads(meta_path.read_text(encoding="utf-8-sig"))
            except (json.JSONDecodeError, OSError):
                metadata[suite] = {}
        else:
            metadata[suite] = {}

    if not frames:
        raise SystemExit(
            f"No all.csv files found in {results_dir}. "
            "Run scripts/run_benchmarks.ps1 first."
        )

    return pd.concat(frames, ignore_index=True), metadata


def aggregate(frame: pd.DataFrame) -> pd.DataFrame:
    """Aggregate repetitions into medians and retain min/max throughput."""
    grouped = (
        frame.groupby(
            ["suite", "language", "primitive", "scenario", "threads"],
            as_index=False,
        )
        .agg(
            thr=("throughput_ops_s", "median"),
            thr_min=("throughput_ops_s", "min"),
            thr_max=("throughput_ops_s", "max"),
            p50=("lat_p50_ns", "median"),
            p95=("lat_p95_ns", "median"),
            p99=("lat_p99_ns", "median"),
            p999=("lat_p99_9_ns", "median"),
            cpu=("cpu_percent", "median"),
            rss=("rss_peak_mb", "median"),
            ops=("ops", "median"),
            batch=("batch", "median"),
            repeats=("repeat", "nunique"),
        )
    )

    grouped["thr_spread"] = grouped["thr_max"] / grouped["thr_min"].replace(0, np.nan)
    grouped["stable"] = grouped["thr_spread"] <= STABILITY_LIMIT

    return grouped.sort_values(
        ["suite", "primitive", "scenario", "language", "threads"]
    ).reset_index(drop=True)


def make_series(rows: pd.DataFrame) -> Series:
    """Create a metric series and calculate speedup relative to one thread."""
    data = rows.sort_values("threads").copy()
    if data.empty:
        raise ValueError("Cannot create a series from an empty set of rows")

    baseline_rows = data[data["threads"] == 1]
    baseline_ok = False
    baseline_throughput = np.nan

    if not baseline_rows.empty:
        baseline = baseline_rows.iloc[0]
        baseline_throughput = float(baseline["thr"])
        baseline_ok = bool(baseline["stable"]) and baseline_throughput > 0

    if baseline_ok:
        data["speedup"] = data["thr"] / baseline_throughput
    else:
        data["speedup"] = np.nan

    data["efficiency"] = data["speedup"] / data["threads"]

    data["cpu_util"] = np.where(
        data["cpu"] > 0,
        data["cpu"] / (data["threads"] * CPU_PERCENT),
        np.nan,
    )

    return Series(
        language=str(data["language"].iloc[0]),
        primitive=str(data["primitive"].iloc[0]),
        scenario=str(data["scenario"].iloc[0]),
        data=data,
        baseline_ok=baseline_ok,
    )


def build_pairs(aggregated: pd.DataFrame, suite: str) -> list[Pair]:
    """Match C++ and Rust according to the selected suite's rules."""
    frame = aggregated[aggregated["suite"] == suite]
    pairs: list[Pair] = []

    if suite == "libraries":
        for cpp_primitive, rust_primitive in LIBRARY_PAIRS:
            cpp_rows = frame[
                (frame["language"] == "cpp")
                & (frame["primitive"] == cpp_primitive)
            ]
            rust_rows = frame[
                (frame["language"] == "rust")
                & (frame["primitive"] == rust_primitive)
            ]

            common_scenarios = sorted(
                set(cpp_rows["scenario"]) & set(rust_rows["scenario"])
            )
            for scenario in common_scenarios:
                series = [
                    make_series(cpp_rows[cpp_rows["scenario"] == scenario]),
                    make_series(rust_rows[rust_rows["scenario"] == scenario]),
                ]
                pairs.append(
                    Pair(
                        title=f"C++ {cpp_primitive}  ↔  Rust {rust_primitive}",
                        scenario=str(scenario),
                        series=series,
                    )
                )
    else:
        cpp_rows = frame[frame["language"] == "cpp"]
        rust_rows = frame[frame["language"] == "rust"]

        cpp_keys = set(zip(cpp_rows["primitive"], cpp_rows["scenario"]))
        rust_keys = set(zip(rust_rows["primitive"], rust_rows["scenario"]))

        for primitive, scenario in sorted(cpp_keys & rust_keys):
            cpp_pair_rows = cpp_rows[
                (cpp_rows["primitive"] == primitive)
                & (cpp_rows["scenario"] == scenario)
            ]
            rust_pair_rows = rust_rows[
                (rust_rows["primitive"] == primitive)
                & (rust_rows["scenario"] == scenario)
            ]
            pairs.append(
                Pair(
                    title=f"{primitive} / {scenario}",
                    scenario=str(scenario),
                    series=[make_series(cpp_pair_rows), make_series(rust_pair_rows)],
                )
            )

    return pairs