# cpp-rust-concurrent-structures

Benchmark-driven comparison of lock-free and wait-free concurrent structures in C++ and Rust.

## Requirements

- Python 3.11 or newer
- Bazel (version from [.bazelversion](.bazelversion))
- Rust toolchain
- MSVC Build Tools with C++ support (Windows)

Run all commands from the repository root in PowerShell.

## Running the scripts

### Benchmarks

Build and run the C++ and Rust benchmarks from the `primitives` and `libraries` suites:

```powershell
python -m scripts.run --suite all
```

Run a single suite:

```powershell
python -m scripts.run --suite primitives
python -m scripts.run --suite libraries
```

The default suite is `primitives`. Results are written to
`results/<run_id>/<suite>/`, with one CSV per benchmark plus a merged
`all.csv` and `meta.json`.

Common options:

| Option | Default | Description |
| --- | --- | --- |
| `--suite` | `primitives` | `primitives`, `libraries`, or `all` |
| `--threads` | `2 4 8 16 32` | Thread counts |
| `--duration` | `1.0` | Benchmark duration in seconds |
| `--repeats` | `10` | Number of repetitions |
| `--batch` | `100` | Operations per batch |
| `--warmup` | `1.0` | Warmup duration in seconds |
| `--only` | — | Run only selected benchmarks |
| `--results-dir` | `results` | Output directory |
| `--compilation-mode` | `opt` | `fastbuild`, `dbg`, or `opt` |

Example:

```powershell
python -m scripts.run --suite all --threads 2 4 8 --duration 2 --repeats 5
```

### Plotting

Generate charts and reports from benchmark results. The plotting script
requires `matplotlib`, `numpy`, and `pandas`:

```powershell
pip install "matplotlib>=3.8" "numpy>=1.26" "pandas>=2.1"
```

Point `--results` at a run directory produced by the benchmark script:

```powershell
python -m scripts.plot.benchmark --results results/<run_id> --suite all
```

Common options:

| Option | Default | Description |
| --- | --- | --- |
| `--results` | `results` | Directory with `primitives/` and `libraries/` subdirectories |
| `--suite` | `all` | `primitives`, `libraries`, or `all` |
| `--out` | `<results>/plots` | Output directory for plots |
| `--formats` | `png` | Comma-separated formats: `png,svg,pdf` |
| `--dpi` | `150` | DPI for raster formats |
| `--physical` | from `meta.json` | Number of physical cores |
| `--logical` | from `meta.json` | Number of logical cores |
| `--bar-threads` | — | Thread counts for bar charts, e.g. `1,16,32` |

## Services

The `cpp/services` package contains trade-processing TCP services and a
load generator. Build and run them with Bazel.

| Target | Port | Description |
| --- | --- | --- |
| `//cpp/services/blocking:blocking_service` | 9001 | Trade server backed by a blocking queue |
| `//cpp/services/lockfree:lockfree_service` | 9002 | Trade server backed by a lock-free queue |
| `//cpp/services/bombardier:bombardier` | — | Trade load generator (client) |

Start a service:

```powershell
bazel run //cpp/services/blocking:blocking_service
bazel run //cpp/services/lockfree:lockfree_service
```

Run the load generator against one or both services. Pass arguments after
`--` so Bazel forwards them to the binary:

```powershell
bazel run //cpp/services/bombardier:bombardier -- --target 127.0.0.1:9001 --target 127.0.0.1:9002 --duration 10
```

`--target` accepts a `host:port` endpoint and may be repeated;
`--duration` sets the run duration in seconds (`0` = unlimited).
