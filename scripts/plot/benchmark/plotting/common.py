"""Shared plot styling and saving helpers."""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

from ..formatters import fmt_si
from ..models import Context, PanelSpec


def mark_topology(ax: plt.Axes, physical: int, logical: int) -> None:
    """Mark physical cores and the SMT range on the thread-count axis."""
    if physical < logical:
        ax.axvspan(
            physical,
            logical,
            color="0.85",
            alpha=0.45,
            zorder=0,
            linewidth=0,
        )
        ax.axvline(
            physical,
            color="0.45",
            linestyle=":",
            linewidth=1.0,
            zorder=1,
        )
        ax.annotate(
            "SMT",
            xy=(physical + (logical - physical) / 2, 0.97),
            xycoords=("data", "axes fraction"),
            ha="center",
            va="top",
            fontsize=8,
            color="0.35",
        )


def setup_axis(ax: plt.Axes, spec: PanelSpec, ctx: Context) -> None:
    ax.set_title(spec.title, fontsize=11, fontweight="bold")
    ax.set_xlabel("Threads")
    ax.set_ylabel(spec.ylabel)
    ax.set_xscale("log", base=2)
    ax.set_xticks(ctx.threads)
    ax.set_xticklabels([str(thread) for thread in ctx.threads], fontsize=8)
    ax.minorticks_off()

    if spec.logy:
        ax.set_yscale("log")
        ax.yaxis.set_major_formatter(FuncFormatter(spec.yfmt or fmt_si))
    elif spec.yfmt is not None:
        ax.yaxis.set_major_formatter(FuncFormatter(spec.yfmt))

    mark_topology(ax, ctx.physical, ctx.logical)
    ax.grid(True, which="major", alpha=0.25, linewidth=0.6)
    ax.grid(True, which="minor", alpha=0.12, linewidth=0.4)


def save_figure(fig: plt.Figure, ctx: Context, stem: str) -> list[Path]:
    paths: list[Path] = []
    for image_format in ctx.formats:
        path = ctx.out_dir / f"{stem}.{image_format}"
        fig.savefig(
            path,
            dpi=ctx.dpi,
            format=image_format,
            bbox_inches="tight",
        )
        paths.append(path)
    return paths