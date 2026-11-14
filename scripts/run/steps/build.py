import subprocess
from pathlib import Path


def build_suite(suite: str, bazel: str, compilation_mode: str, project_root: Path) -> Path:
    targets = [ f"//cpp/benchmarks/{suite}:all", f"//rust/benchmarks/{suite}:all" ]

    command = [ bazel, "build", f"--compilation_mode={compilation_mode}", *targets ]

    print(f"\nBuilding suite: {suite}")

    result = subprocess.run(command, cwd=project_root, check=False)

    if result.returncode != 0:
        raise RuntimeError(f"Bazel build failed with exit code {result.returncode}.")

    result = subprocess.run(
        [ bazel, "info", "bazel-bin", f"--compilation_mode={compilation_mode}" ],
        cwd=project_root,
        capture_output=True,
        text=True,
        check=False
    )

    if result.returncode != 0:
        raise RuntimeError("Failed to determine Bazel binary directory.")

    bin_dir = Path(result.stdout.strip())

    if not bin_dir.is_absolute():
        bin_dir = project_root / bin_dir

    if not bin_dir.exists():
        raise RuntimeError(f"Bazel binary directory not found: {bin_dir}")

    return bin_dir