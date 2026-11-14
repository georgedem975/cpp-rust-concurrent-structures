import os
from datetime import datetime
from pathlib import Path

from .steps.build import build_suite
from .steps.config import parse_args
from .steps.execution import (build_command, calculate_timeout, run_benchmark)
from .steps.metadata import create_metadata, write_metadata
from .steps.results import count_csv_rows, merge_csv
from .steps.suites import select_benchmarks


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def get_executable(bin_dir: Path, language: str, suite: str, benchmark: str) -> Path:
    extension = ".exe" if os.name == "nt" else ""

    return (bin_dir / language / "benchmarks" / suite / f"bench_{benchmark}{extension}")


def run_suite(args, suite: str, run_root: Path, run_id: str) -> int:
    selected = select_benchmarks(suite, args.only)

    if not any(selected.values()):
        print(f"\nNo benchmarks selected for suite: {suite}")
        return 0

    output_dir = run_root / suite
    output_dir.mkdir(parents=True, exist_ok=True)

    metadata = create_metadata(args, suite, run_id)
    metadata["selection"] = selected

    metadata_path = output_dir / "meta.json"
    write_metadata(metadata_path, metadata)

    try:
        bin_dir = build_suite(suite, args.bazel, args.compilation_mode, PROJECT_ROOT)

    except RuntimeError as error:
        print(f"\nBuild error: {error}")

        metadata["status"] = "build_failed"
        metadata["build_error"] = str(error)
        write_metadata(metadata_path, metadata)

        return 1

    csv_paths = []

    for language, benchmarks in selected.items():
        for benchmark in benchmarks:
            print(f"\nRunning: {language}/{benchmark}")

            executable = get_executable(bin_dir, language, suite, benchmark)

            entry = {
                "language": language,
                "benchmark": benchmark,
                "status": "pending",
                "executable": str(executable)
            }

            if not executable.is_file():
                entry["status"] = "missing_binary"
                metadata["benchmarks"].append(entry)
                write_metadata(metadata_path, metadata)

                print(f"Executable not found: {executable}")
                continue

            csv_path = output_dir / f"{language}_{benchmark}.csv"

            command = build_command(executable, csv_path, args)
            timeout = calculate_timeout(args)

            print(f"Timeout: {timeout:.1f} seconds")

            result = run_benchmark(command, timeout, PROJECT_ROOT)

            entry.update(result)

            if result["status"] == "completed":
                if csv_path.is_file():
                    entry["csv_rows"] = count_csv_rows(csv_path)
                    csv_paths.append(csv_path)
                else:
                    entry["status"] = "missing_csv"
                    print("Benchmark finished without producing CSV.")

            metadata["benchmarks"].append(entry)
            write_metadata(metadata_path, metadata)

            print(f"Status: {entry['status']}")

    try:
        merged_rows = merge_csv(csv_paths, output_dir / "all.csv")

        metadata["merged_csv"] = {
            "path": "all.csv",
            "rows": merged_rows
        }

    except (OSError, ValueError) as error:
        metadata["merge_error"] = str(error)
        print(f"\nCSV merge error: {error}")

    failed = any(
        entry["status"] != "completed"
        for entry in metadata["benchmarks"]
    )

    metadata["status"] = (
        "completed_with_errors"
        if failed or "merge_error" in metadata
        else "completed"
    )

    write_metadata(metadata_path, metadata)

    print(f"\nSuite finished: {suite}")
    print(f"Results: {output_dir}")

    return 1 if metadata["status"] != "completed" else 0


def main() -> int:
    args = parse_args()

    run_id = datetime.now().astimezone().strftime("%Y%m%d_%H%M%S_%f")

    results_dir = args.results_dir

    if not results_dir.is_absolute():
        results_dir = PROJECT_ROOT / results_dir

    run_root = results_dir / run_id
    run_root.mkdir(parents=True, exist_ok=True)

    suites = (
        ["primitives", "libraries"]
        if args.suite == "all"
        else [args.suite]
    )

    exit_code = 0

    print(f"Run ID: {run_id}")
    print(f"Results directory: {run_root}")

    for suite in suites:
        result = run_suite(args, suite, run_root, run_id)

        if result != 0:
            exit_code = 1

    print("\nBenchmark execution finished.")

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())