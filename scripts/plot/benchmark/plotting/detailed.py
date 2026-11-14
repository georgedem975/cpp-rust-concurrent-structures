"""Detailed plots for one matched C++ and Rust pair."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from ..config import LANG_STYLE
from ..formatters import fmt_integer, fmt_ns, fmt_percent, fmt_si
from ..models import Context, Pair, PanelSpec, Series
from .common import save_figure, setup_axis

PANELS = [
    PanelSpec(
        key="thr",
        title="Throughput (median, band = min..max)",
        ylabel="Operations/s",
        logy=True,
        yfmt=fmt_si,
    ),
    PanelSpec(
        key="speedup",
        title="Speedup  X(N)/X(1)",
        ylabel="×",
        yfmt=fmt_integer,
        ideal="linear",
    ),
    PanelSpec(
        key="efficiency",
        title="Efficiency  X(N)/(N·X(1))",
        ylabel="fraction of ideal",
        yfmt=fmt_percent,
        ideal="one",
    ),
    PanelSpec(
        key="p50",
        title="Median operation latency (p50)",
        ylabel="per operation",
        logy=True,
        yfmt=fmt_ns,
    ),
    PanelSpec(
        key="p99",
        title="Tail operation latency (p99)",
        ylabel="per operation",
        logy=True,
        yfmt=fmt_ns,
    ),
    PanelSpec(
        key="cpu_util",
        title="CPU utilization (fraction of busy time per thread)",
        ylabel="fraction",
        yfmt=fmt_percent,
        ideal="cpu",
    ),
]


def panel_values(
    series: Series,
    spec: PanelSpec,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    data = series.data
    x = data["threads"].to_numpy(dtype=float)

    if spec.key == "thr":
        y = data["thr"].to_numpy(dtype=float)
        lower = data["thr_min"].to_numpy(dtype=float)
        upper = data["thr_max"].to_numpy(dtype=float)
    else:
        y = data[spec.key].to_numpy(dtype=float)
        lower = upper = y

    if spec.logy:
        y = np.clip(y, 1e-3, None)
        lower = np.clip(lower, 1e-3, None)
        upper = np.clip(upper, 1e-3, None)

    return x, y, lower, upper


def plot_pair(pair: Pair, ctx: Context, index: int) -> list[Path]:
    """Plot six panels for a pair of structures and save the figure."""
    fig, axes = plt.subplots(
        2,
        3,
        figsize=(19, 10.5),
        constrained_layout=True,
    )
    flat_axes = axes.ravel()
    max_threads = max(ctx.threads)

    for ax, spec in zip(flat_axes, PANELS):
        plotted = False
        skipped: list[Series] = []

        for series in pair.series:
            style = LANG_STYLE[series.language]
            x, y, lower, upper = panel_values(series, spec)

            if np.all(np.isnan(y)):
                skipped.append(series)
                continue

            plotted = True
            ax.plot(
                x,
                y,
                marker=style["marker"],
                markersize=5,
                linewidth=1.9,
                color=style["color"],
                label=series.label,
                zorder=3,
            )

            if spec.key == "thr" and np.any(upper > lower):
                ax.fill_between(
                    x,
                    lower,
                    upper,
                    color=style["color"],
                    alpha=0.18,
                    linewidth=0,
                    zorder=1,
                )

            unstable = series.data[~series.data["stable"].astype(bool)]
            if spec.key == "thr" and not unstable.empty:
                ax.scatter(
                    unstable["threads"],
                    np.clip(unstable["thr"], 1e-3, None),
                    marker="x",
                    s=55,
                    linewidths=2.0,
                    color="black",
                    zorder=5,
                    label=f"{style['label']}: unstable",
                )

        if spec.ideal == "linear":
            ax.plot(
                [1, max_threads],
                [1, max_threads],
                color="0.25",
                linestyle="--",
                linewidth=1.2,
                label="Ideal",
                zorder=2,
            )
        elif spec.ideal == "one":
            ax.axhline(
                1.0,
                color="0.25",
                linestyle="--",
                linewidth=1.2,
                label="Ideal",
                zorder=2,
            )
        elif spec.ideal == "cpu":
            ax.axhline(
                1.0,
                color="0.25",
                linestyle="--",
                linewidth=1.2,
                label="100% (no idle time)",
                zorder=2,
            )

        setup_axis(ax, spec, ctx)

        if not plotted:
            invalid_baselines = [item.title for item in pair.series if not item.baseline_ok]
            message = "No valid N=1 baseline measurement:\n" + ", ".join(
                invalid_baselines
            )
            ax.text(
                0.5,
                0.55,
                message,
                transform=ax.transAxes,
                ha="center",
                va="center",
                fontsize=9,
                color="0.35",
                bbox={
                    "boxstyle": "round,pad=0.4",
                    "facecolor": "white",
                    "edgecolor": "0.7",
                },
            )
        elif skipped:
            ax.text(
                0.02,
                0.03,
                "Not shown (invalid N=1 baseline): "
                + ", ".join(item.short for item in skipped),
                transform=ax.transAxes,
                fontsize=7.5,
                color="0.35",
                va="bottom",
            )

        ax.legend(fontsize=8, loc="best", framealpha=0.9)

    metadata = ctx.suite_meta
    subtitle = metadata.get("cpu", "").strip()
    extra: list[str] = []
    if subtitle:
        extra.append(subtitle)
    extra.append(f"Scenario: {pair.scenario}")

    if metadata.get("duration_s") is not None:
        extra.append(
            f"{metadata.get('duration_s')} s × {metadata.get('repeats')} repetitions, "
            f"versions: MSVC {metadata.get('msvc_version', '—')}"
        )

    fig.suptitle(
        f"{pair.title}\n{' · '.join(extra)}",
        fontsize=13,
        fontweight="bold",
    )

    filename = f"{index:02d}_{pair.slug}"
    paths = save_figure(fig, ctx, filename)
    plt.close(fig)
    return paths