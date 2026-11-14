import argparse
import math
from pathlib import Path
from typing import Sequence

from .suites import SUITES


DEFAULT_THREADS = [2, 4, 8, 16, 32]


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run C++ and Rust benchmarks.")

    parser.add_argument(
        "--suite",
        choices=["primitives", "libraries", "all"],
        default="primitives",
        help="Benchmark suite."
    )

    parser.add_argument(
        "--threads",
        nargs="+",
        type=int,
        default=DEFAULT_THREADS,
        help="Thread counts."
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=1.0,
        help="Benchmark duration in seconds."
    )

    parser.add_argument(
        "--repeats",
        type=int,
        default=10,
        help="Number of repetitions."
    )

    parser.add_argument(
        "--batch",
        type=int,
        default=100,
        help="Operations per batch."
    )

    parser.add_argument(
        "--warmup",
        type=float,
        default=1.0,
        help="Warmup duration in seconds."
    )

    parser.add_argument(
        "--results-dir",
        type=Path,
        default=Path("results"),
        help="Directory for benchmark results."
    )

    parser.add_argument(
        "--only",
        nargs="+",
        default=None,
        help="Run only selected benchmarks."
    )

    parser.add_argument(
        "--bazel",
        default="bazel",
        help="Bazel executable."
    )

    parser.add_argument(
        "--compilation-mode",
        choices=["fastbuild", "dbg", "opt"],
        default="opt",
        help="Bazel compilation mode."
    )

    args = parser.parse_args(argv)

    validate_args(parser, args)

    return args


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if not args.threads or any(thread <= 0 for thread in args.threads):
        parser.error("Thread counts must be positive integers.")

    if len(args.threads) != len(set(args.threads)):
        parser.error("Thread counts must not contain duplicates.")

    if not math.isfinite(args.duration) or args.duration <= 0:
        parser.error("Duration must be a positive finite number.")

    if not math.isfinite(args.warmup) or args.warmup < 0:
        parser.error("Warmup must be a non-negative finite number.")

    if args.repeats <= 0:
        parser.error("Repeats must be positive.")

    if args.batch <= 0:
        parser.error("Batch must be positive.")

    if args.only is not None:
        args.only = list(dict.fromkeys(args.only))

        suites = (
            ("primitives", "libraries")
            if args.suite == "all"
            else (args.suite,)
        )

        available = {
            benchmark
            for suite in suites
            for language_benchmarks in SUITES[suite].values()
            for benchmark in language_benchmarks
        }

        unknown = set(args.only) - available

        if unknown:
            parser.error(f"Unknown benchmarks: {', '.join(sorted(unknown))}")