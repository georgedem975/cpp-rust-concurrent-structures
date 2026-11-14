import csv
from pathlib import Path


def count_csv_rows(path: Path) -> int:
    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.reader(file)

        try:
            next(reader)
        except StopIteration:
            return 0

        return sum(1 for _ in reader)


def merge_csv(source_paths: list[Path], destination: Path) -> int:
    if not source_paths:
        return 0

    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = destination.with_suffix(".tmp")

    total_rows = 0
    expected_header = None

    try:
        with temporary_path.open("w", encoding="utf-8", newline="") as output_file:
            writer = csv.writer(output_file)

            for source_path in source_paths:
                with source_path.open("r", encoding="utf-8", newline="") as input_file:
                    reader = csv.reader(input_file)
                    header = next(reader, None)

                    if header is None:
                        continue

                    if expected_header is None:
                        expected_header = header
                        writer.writerow(header)

                    elif header != expected_header:
                        raise ValueError(f"CSV headers do not match: {source_path}")

                    for row in reader:
                        if len(row) != len(expected_header):
                            raise ValueError(f"Invalid CSV row in {source_path}")

                        writer.writerow(row)
                        total_rows += 1

        if expected_header is None:
            temporary_path.unlink(missing_ok=True)
            return 0

        temporary_path.replace(destination)

    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise

    return total_rows