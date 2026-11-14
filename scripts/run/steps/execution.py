import subprocess
import time
from pathlib import Path


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
    metric_count = 2
    scenario_count = 4
    estimated_seconds = scenario_count * (
        metric_count * args.warmup
        + args.repeats
        * len(args.threads)
        * metric_count
        * (args.duration + 0.2)
    )

    return max(60.0, estimated_seconds * 1.5 + 120.0)


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