"""Orchestrate the plotting and reporting pipeline."""

from __future__ import annotations

import matplotlib
import numpy as np

from .config import DEFAULT_LOGICAL, DEFAULT_PHYSICAL, SUITE_TITLES
from .data import aggregate, build_pairs, load_csv
from .formatters import fmt_pct_cell, fmt_speedup_cell
from .models import Context
from .plotting.detailed import plot_pair
from .plotting.summary import (
    build_matrix,
    plot_heatmap,
    plot_memory,
    plot_throughput_bars,
)
from .report import build_summary, report_quality, write_summary

SPEEDUP_BASELINE = 1.0
MIN_SPEEDUP_FLOOR = 0.05
MIN_SPEEDUP_VMAX = 1.01


def run(args) -> int:
    """Run the full processing pipeline for one or more suites."""
    results_dir = args.results.expanduser()
    suites = ["primitives", "libraries"] if args.suite == "all" else [args.suite]
    output_root = args.out.expanduser() if args.out else results_dir / "plots"

    raw_data, metadata_by_suite = load_csv(results_dir, suites)

    aggregated = aggregate(raw_data)
    actual_suites = sorted(aggregated["suite"].unique())
    print(
        f"Loaded {len(raw_data)} rows from suites: {', '.join(actual_suites)}"
    )

    quality_notes = report_quality(aggregated)
    if quality_notes:
        print(f"[!] Data quality notes: {len(quality_notes)}")
        for kind, message in quality_notes:
            print(f"    [{kind}] {message}")

    all_paths = []

    for suite in [item for item in suites if item in set(actual_suites)]:
        suite_metadata = metadata_by_suite.get(suite, {})
        physical = int(
            args.physical or suite_metadata.get("physical_cpu") or DEFAULT_PHYSICAL
        )
        logical = int(
            args.logical or suite_metadata.get("logical_cpu") or DEFAULT_LOGICAL
        )
        threads = sorted(
            int(thread_count)
            for thread_count in aggregated.loc[
                aggregated["suite"] == suite, "threads"
            ].unique()
        )

        output_dir = output_root / suite
        output_dir.mkdir(parents=True, exist_ok=True)

        context = Context(
            out_dir=output_dir,
            threads=threads,
            physical=physical,
            logical=logical,
            dpi=args.dpi,
            formats=[
                image_format.strip().lower()
                for image_format in args.formats.split(",")
                if image_format.strip()
            ],
            suite_meta=suite_metadata,
        )

        pairs = build_pairs(aggregated, suite)
        if not pairs:
            print(f"[!] No C++/Rust pairs found for suite {suite}")
            continue

        print(
            f"\n== {suite}: {len(pairs)} pairs, threads {threads}, "
            f"cores {physical}/{logical}"
        )

        for index, pair in enumerate(pairs, start=1):
            all_paths.extend(plot_pair(pair, context, index))
            print(f"   {pair.title}")

        matrix_rows, speedup = build_matrix(pairs, "speedup", threads)
        finite_speedup = speedup[np.isfinite(speedup)]
        lowest_speedup = float(finite_speedup.min()) if finite_speedup.size else MIN_SPEEDUP_FLOOR
        speedup_norm = matplotlib.colors.TwoSlopeNorm(
            vcenter=SPEEDUP_BASELINE,
            vmin=min(MIN_SPEEDUP_FLOOR, lowest_speedup),
            vmax=max(float(max(threads)), MIN_SPEEDUP_VMAX),
        )
        all_paths.extend(
            plot_heatmap(
                matrix_rows,
                speedup,
                threads,
                f"Speedup X(N)/X(1) — {SUITE_TITLES.get(suite, suite)}",
                fmt_cell=fmt_speedup_cell,
                out_stem="summary_speedup_heatmap",
                ctx=context,
                norm=speedup_norm,
            )
        )

        cpu_rows, cpu_utilization = build_matrix(pairs, "cpu_util", threads)
        all_paths.extend(
            plot_heatmap(
                cpu_rows,
                cpu_utilization,
                threads,
                "CPU utilization (fraction of busy time per thread) — "
                f"{SUITE_TITLES.get(suite, suite)}",
                fmt_cell=fmt_pct_cell,
                out_stem="summary_cpu_heatmap",
                ctx=context,
                norm=matplotlib.colors.Normalize(vmin=0.0, vmax=1.0),
            )
        )

        if args.bar_threads:
            focus_threads = [
                int(value)
                for value in args.bar_threads.split(",")
                if value.strip()
            ]
        else:
            focus_threads = sorted(
                {
                    thread_count
                    for thread_count in (1, physical, max(threads))
                    if thread_count in threads
                }
            )

        all_paths.extend(plot_throughput_bars(pairs, context, focus_threads))
        all_paths.extend(plot_memory(pairs, context))

        summary = build_summary(pairs)
        suite_notes = [
            note for note in quality_notes if note[1].startswith(f"{suite}:")
        ]
        all_paths.extend(write_summary(summary, output_dir, suite_notes))

    print(f"\nDone: {len(all_paths)} files written to {output_root}")
    for path in all_paths:
        print(f"   {path}")

    return 0