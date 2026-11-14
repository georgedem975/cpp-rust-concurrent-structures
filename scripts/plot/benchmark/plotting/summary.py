"""Summary plots: heatmaps, throughput, and memory."""

from __future__ import annotations

from pathlib import Path
from typing import Callable, Sequence

import matplotlib
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter

from ..config import LANG_STYLE
from ..formatters import fmt_mb, fmt_pct_cell, fmt_si, fmt_speedup_cell
from ..models import Context, Pair
from .common import save_figure


def build_matrix(
    pairs: Sequence[Pair],
    key: str,
    threads: Sequence[int],
) -> tuple[list[str], np.ndarray]:
    rows: list[str] = []
    values: list[list[float]] = []

    for pair in pairs:
        for series in pair.series:
            rows.append(series.title)
            row_values: list[float] = []
            for thread_count in threads:
                row = series.data[series.data["threads"] == thread_count]

                value = (
                    float(row[key].iloc[0])
                    if not row.empty
                    else np.nan
                )
                row_values.append(value)

            values.append(row_values)

    return rows, np.array(values, dtype=float)


def plot_heatmap(
    rows: Sequence[str],
    values: np.ndarray,
    threads: Sequence[int],
    title: str,
    fmt_cell: Callable[[float], str],
    out_stem: str,
    ctx: Context,
    norm=None,
    cmap: str = "RdYlGn",
) -> list[Path]:
    height = max(3.2, 0.42 * len(rows) + 2.0)
    fig, ax = plt.subplots(figsize=(11, height), constrained_layout=True)

    masked = np.ma.masked_invalid(values)
    image = ax.imshow(masked, aspect="auto", cmap=cmap, norm=norm)
    ax.set_xticks(range(len(threads)))
    ax.set_xticklabels([str(thread) for thread in threads])
    ax.set_yticks(range(len(rows)))
    ax.set_yticklabels(rows, fontsize=8)
    ax.set_xlabel("Threads")
    ax.set_title(title, fontsize=12, fontweight="bold")

    for row_index in range(values.shape[0]):
        for column_index in range(values.shape[1]):
            value = values[row_index, column_index]
            if not np.isfinite(value):
                ax.text(
                    column_index,
                    row_index,
                    "—",
                    ha="center",
                    va="center",
                    fontsize=8,
                    color="0.45",
                )

                continue

            position = float(norm(value)) if norm is not None else 0.5
            text_color = (
                "white"
                if position < 0.18 or position > 0.85
                else "black"
            )
            ax.text(
                column_index,
                row_index,
                fmt_cell(value),
                ha="center",
                va="center",
                fontsize=8,
                color=text_color,
            )

    fig.colorbar(image, ax=ax, shrink=0.85)

    if ctx.physical in list(threads):
        physical_index = list(threads).index(ctx.physical)
        ax.axvline(
            physical_index + 0.5,
            color="black",
            linestyle="--",
            linewidth=1.0,
        )
        ax.text(
            1.0,
            1.005,
            f"SMT → (after {ctx.physical} threads)",
            transform=ax.transAxes,
            ha="right",
            va="bottom",
            fontsize=7.5,
            color="0.3",
        )

    paths = save_figure(fig, ctx, out_stem)

    plt.close(fig)

    return paths


def plot_throughput_bars(
    pairs: Sequence[Pair],
    ctx: Context,
    threads_focus: Sequence[int],
) -> list[Path]:
    """Plot grouped throughput bars."""
    if not threads_focus:
        return []

    labels = [pair.title for pair in pairs]
    group_count = len(threads_focus)
    x = np.arange(len(pairs))
    width = 0.8 / (2 * group_count)
    fig, ax = plt.subplots(
        figsize=(max(12, 1.5 * len(pairs)), 7),
        constrained_layout=True,
    )
    shades = np.linspace(0.45, 1.0, group_count)

    for language in ("cpp", "rust"):
        style = LANG_STYLE[language]

        for group_index, thread_count in enumerate(threads_focus):
            values: list[float] = []

            for pair in pairs:
                series = next(
                    (item for item in pair.series if item.language == language),
                    None,
                )
                value = np.nan
                if series is not None:
                    row = series.data[series.data["threads"] == thread_count]

                    if not row.empty:
                        value = float(row["thr"].iloc[0])

                values.append(value)

            rgb = matplotlib.colors.to_rgb(style["color"])
            shade = tuple(
                0.35 + (component - 0.35) * shades[group_index]
                for component in rgb
            )
            offset = (0 if language == "cpp" else group_count) + group_index
            ax.bar(
                x + (offset - (group_count - 0.5)) * width,
                values,
                width=width,
                color=shade,
                edgecolor="white",
                linewidth=0.5,
                label=f"{style['label']}, {thread_count} threads",
            )

    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(FuncFormatter(fmt_si))
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=25, ha="right", fontsize=8)
    ax.set_ylabel("Operations/s (log scale)")
    ax.set_title(
        "Throughput: C++ vs. Rust\n"
        + ", ".join(f"{thread_count} threads" for thread_count in threads_focus),
        fontsize=12,
        fontweight="bold",
    )
    ax.grid(True, axis="y", alpha=0.25)
    ax.legend(fontsize=8, ncol=2)

    paths = save_figure(fig, ctx, "summary_throughput_bars")

    plt.close(fig)

    return paths


def plot_memory(pairs: Sequence[Pair], ctx: Context) -> list[Path]:
    fig, ax = plt.subplots(figsize=(10, 6.5), constrained_layout=True)

    rows: list[tuple[str, float, float]] = []

    for pair in pairs:
        cpp = next((item for item in pair.series if item.language == "cpp"), None)
        rust = next((item for item in pair.series if item.language == "rust"), None)

        if cpp is None or rust is None:
            continue

        rows.append(
            (
                pair.title,
                float(cpp.data["rss"].median()),
                float(rust.data["rss"].median()),
            )
        )

    labels = [row[0] for row in rows]
    indices = np.arange(len(rows))
    bar_width = 0.38

    ax.bar(
        indices - bar_width / 2,
        [max(row[1], 1e-3) for row in rows],
        bar_width,
        color=LANG_STYLE["cpp"]["color"],
        label="C++",
    )
    ax.bar(
        indices + bar_width / 2,
        [max(row[2], 1e-3) for row in rows],
        bar_width,
        color=LANG_STYLE["rust"]["color"],
        label="Rust",
    )

    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(FuncFormatter(fmt_mb))
    ax.set_xticks(indices)
    ax.set_xticklabels(labels, rotation=25, ha="right", fontsize=8)
    ax.set_ylabel("MB (log scale)")
    ax.set_title(
        "Peak process RSS, MB (includes the benchmark's own buffers!)",
        fontsize=11,
        fontweight="bold",
    )
    ax.grid(True, axis="y", alpha=0.25)
    ax.legend(fontsize=9)

    fig.suptitle("Memory: peak RSS", fontsize=13, fontweight="bold")
    paths = save_figure(fig, ctx, "summary_memory")

    plt.close(fig)

    return paths