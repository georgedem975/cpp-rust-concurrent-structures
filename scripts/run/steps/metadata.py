import ctypes
import json
import os
import platform
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def get_total_memory_gb() -> float | None:
    if os.name != "nt":
        return None

    class MemoryStatus(ctypes.Structure):
        _fields_ = [
            ("dwLength", ctypes.c_ulong),
            ("dwMemoryLoad", ctypes.c_ulong),
            ("ullTotalPhys", ctypes.c_ulonglong),
            ("ullAvailPhys", ctypes.c_ulonglong),
            ("ullTotalPageFile", ctypes.c_ulonglong),
            ("ullAvailPageFile", ctypes.c_ulonglong),
            ("ullTotalVirtual", ctypes.c_ulonglong),
            ("ullAvailVirtual", ctypes.c_ulonglong),
            ("ullAvailExtendedVirtual", ctypes.c_ulonglong)
        ]

    try:
        status = MemoryStatus()
        status.dwLength = ctypes.sizeof(status)

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        get_memory_status = kernel32.GlobalMemoryStatusEx
        get_memory_status.argtypes = [ctypes.POINTER(MemoryStatus)]
        get_memory_status.restype = ctypes.c_int

        if get_memory_status(ctypes.byref(status)):
            return round(status.ullTotalPhys / (1024 ** 3), 2)

    except (AttributeError, OSError):
        pass

    return None


def get_bazel_version(bazel: str) -> str | None:
    try:
        result = subprocess.run([bazel, "--version"], capture_output=True, text=True, timeout=10, check=False)

        if result.returncode == 0:
            return result.stdout.strip()

    except (OSError, subprocess.TimeoutExpired):
        pass

    return None


def create_metadata(args, suite: str, run_id: str) -> dict:
    return {
        "run_id": run_id,
        "created_at": datetime.now().astimezone().isoformat(timespec="seconds"),
        "suite": suite,
        "status": "running",
        "system": {
            "os": platform.platform(),
            "python": platform.python_version(),
            "cpu": platform.processor() or platform.machine(),
            "logical_cores": os.cpu_count(),
            "memory_gb": get_total_memory_gb()
        },
        "tools": {
            "bazel": get_bazel_version(args.bazel),
            "msvc_path": os.environ.get("BAZEL_VC")
        },
        "parameters": {
            "threads": args.threads,
            "duration": args.duration,
            "repeats": args.repeats,
            "batch": args.batch,
            "warmup": args.warmup,
            "compilation_mode": args.compilation_mode
        },
        "selection": {},
        "benchmarks": []
    }


def write_metadata(path: Path, metadata: dict) -> None:
    temporary_path = path.with_suffix(".tmp")

    temporary_path.write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    temporary_path.replace(path)