"""Command-line interface."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

from .config import DEFAULT_DPI, DEFAULT_FORMATS, DEFAULT_RESULTS_DIR
from .pipeline import run


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot C++ and Rust comparisons from results/<suite>/all.csv",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  benchmark-plot\n"
            "  benchmark-plot --suite libraries\n"
            "  benchmark-plot --out results/plots --formats png,svg --dpi 200"
        ),
    )
    parser.add_argument(
        "--results",
        type=Path,
        default=Path(DEFAULT_RESULTS_DIR),
        help="directory containing suite subdirectories (default: results)",
    )
    parser.add_argument(
        "--suite",
        default="all",
        choices=["all", "primitives", "libraries"],
        help="suite to process (default: all)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="directory for plots (default: <results>/plots)",
    )
    parser.add_argument(
        "--formats",
        default=DEFAULT_FORMATS,
        help="comma-separated formats: png,svg,pdf",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=DEFAULT_DPI,
        help="DPI for raster formats",
    )
    parser.add_argument(
        "--physical",
        type=int,
        default=None,
        help="number of physical cores (default: from meta.json)",
    )
    parser.add_argument(
        "--logical",
        type=int,
        default=None,
        help="number of logical cores (default: from meta.json)",
    )
    parser.add_argument(
        "--bar-threads",
        default=None,
        help="thread counts for bar charts, e.g. 1,16,32",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

    args = parse_args(argv)
    if args.dpi <= 0:
        raise SystemExit("--dpi must be a positive number")
    if args.physical is not None and args.physical <= 0:
        raise SystemExit("--physical must be a positive number")
    if args.logical is not None and args.logical <= 0:
        raise SystemExit("--logical must be a positive number")

    return run(args)