SUITES = {
    "primitives": {
        "cpp": (
            "mutex",
            "rwlock",
            "once",
            "condvar",
            "barrier",
            "thread",
            "atomic_ptr",
            "atomic_flag",
            "coroutine"
        ),
        "rust": (
            "mutex",
            "rwlock",
            "once",
            "condvar",
            "barrier",
            "thread",
            "atomic_ptr",
            "atomic_flag",
            "coroutine"
        )
    },
    "libraries": {
        "cpp": (
            "boost_bounded_queue",
            "boost_queue",
            "cds_msqueue",
            "boost_stack",
            "cds_stack",
            "cds_skiplist"
        ),
        "rust": (
            "array_queue",
            "scc_queue",
            "scc_stack",
            "skiplist"
        )
    }
}


def get_suite_names() -> tuple[str, ...]:
    return tuple(SUITES)


def get_benchmarks(suite: str, language: str) -> tuple[str, ...]:
    return SUITES[suite][language]


def select_benchmarks(suite: str, only: list[str] | None) -> dict[str, list[str]]:
    selected = {}

    for language, benchmarks in SUITES[suite].items():
        selected[language] = [
            name
            for name in benchmarks
            if only is None or name in only
        ]

    return selected