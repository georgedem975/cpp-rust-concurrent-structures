"""Shared settings, structure mappings, and defaults."""

LANG_STYLE = {
    "cpp": {"label": "C++", "color": "#1f77b4", "marker": "o"},
    "rust": {"label": "Rust", "color": "#d62728", "marker": "s"},
}

LIBRARY_PAIRS: list[tuple[str, str]] = [
    ("bounded_queue", "bounded_queue"),
    ("boost_queue", "scc_queue"),
    ("cds_msqueue", "scc_queue"),
    ("boost_stack", "scc_stack"),
    ("cds_stack", "scc_stack"),
    ("cds_skiplist_map", "skipmap"),
    ("cds_skiplist_set", "skipset"),
]

SUITE_TITLES = {
    "primitives": "Synchronization primitives",
    "libraries": "Concurrent library data structures",
}

DEFAULT_PHYSICAL = 16
DEFAULT_LOGICAL = 32

STABILITY_LIMIT = 50.0

DEFAULT_RESULTS_DIR = "results"
DEFAULT_DPI = 150
DEFAULT_FORMATS = "png"