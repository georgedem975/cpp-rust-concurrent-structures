import subprocess
import time
from pathlib import Path


METRIC_COUNT = 2
SCENARIO_COUNT = 4
PER_RUN_OVERHEAD_SECONDS = 0.2
TIMEOUT_SAFETY_FACTOR = 1.5
MIN_TIMEOUT_SECONDS = 60.0
TIMEOUT_BUFFER_SECONDS = 120.0


def build_command(executable: Path, csv_path: Path, args) -> list[str]:
    return [
        str(executable),
        "--threads",
        ",".join(map(str, args.threads)),
        "--duration",
        str(args.duration),
        "--repeats",
        str(args.repeats),
        "--batch",
        str(args.batch),
        "--warmup",
        str(args.warmup),
        "--out",
        str(csv_path.resolve()),
        "--throughput",
        "--latency"
    ]


def calculate_timeout(args) -> float:
    estimated_seconds = SCENARIO_COUNT * (
        METRIC_COUNT * args.warmup
        + args.repeats
        * len(args.threads)
        * METRIC_COUNT
        * (args.duration + PER_RUN_OVERHEAD_SECONDS)
    )

    return max(MIN_TIMEOUT_SECONDS, estimated_seconds * TIMEOUT_SAFETY_FACTOR + TIMEOUT_BUFFER_SECONDS)


def run_benchmark(command: list[str], timeout_seconds: float, project_root: Path) -> dict:
    start_time = time.monotonic()

    try:
        result = subprocess.run(command, cwd=project_root, timeout=timeout_seconds, check=False)

        status = "completed" if result.returncode == 0 else "failed"

        return {
            "status": status,
            "exit_code": result.returncode,
            "elapsed_seconds": round(time.monotonic() - start_time, 3)
        }

    except subprocess.TimeoutExpired:
        return {
            "status": "timeout",
            "exit_code": None,
            "elapsed_seconds": round(time.monotonic() - start_time, 3)
        }

    except OSError as error:
        return {
            "status": "failed",
            "exit_code": None,
            "error": str(error),
            "elapsed_seconds": round(time.monotonic() - start_time, 3)
        }