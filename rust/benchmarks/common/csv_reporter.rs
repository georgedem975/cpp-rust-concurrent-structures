use std::fs::{self, OpenOptions};
use std::io::{self, BufRead, BufReader, Write};
use std::path::Path;

use crate::metrics::{Metric, Row};
use crate::Options;

pub const CSV_HEADER: &str = concat!(
    "language,primitive,scenario,metric,threads,repeat,batch,ops,wall_ns,",
    "throughput_ops_s,lat_avg_ns,lat_p50_ns,lat_p95_ns,lat_p99_ns,",
    "lat_p99_9_ns,lat_max_ns"
);

pub fn emit(options: &Options, rows: &[Row]) -> io::Result<()> {
    match options.out.as_deref() {
        Some(path) => append_to_file(path, options, rows),
        None => {
            let stdout = io::stdout();
            let mut output = stdout.lock();

            write_rows(&mut output, options, rows, true)
        }
    }
}

fn append_to_file(path: &Path, options: &Options, rows: &[Row]) -> io::Result<()> {
    let write_header = validate_existing_file(path)?;

    let mut output = OpenOptions::new().create(true).append(true).open(path)?;

    write_rows(&mut output, options, rows, write_header)?;
    output.flush()
}

fn validate_existing_file(path: &Path) -> io::Result<bool> {
    match fs::metadata(path) {
        Ok(metadata) if metadata.len() > 0 => {
            let mut line = String::new();

            BufReader::new(fs::File::open(path)?).read_line(&mut line)?;

            let header = line
                .trim_end_matches(&['\r', '\n'][..])
                .trim_start_matches('\u{feff}');

            if header != CSV_HEADER {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    format!(
                        "CSV header does not match the current schema: {}",
                        path.display()
                    ),
                ));
            }

            Ok(false)
        }
        Ok(_) => Ok(true),
        Err(error) if error.kind() == io::ErrorKind::NotFound => Ok(true),
        Err(error) => Err(error),
    }
}

fn write_rows(
    output: &mut impl Write,
    options: &Options,
    rows: &[Row],
    write_header: bool,
) -> io::Result<()> {
    if write_header {
        writeln!(output, "{CSV_HEADER}")?;
    }

    for row in rows {
        write_row_metrics(output, options, row)?;
    }

    Ok(())
}

fn write_row_metrics(output: &mut impl Write, options: &Options, row: &Row) -> io::Result<()> {
    if let Some(metric) = row.metric {
        return write_row(output, row, metric);
    }

    if options.throughput {
        write_row(output, row, Metric::Throughput)?;
    }

    if options.latency {
        write_row(output, row, Metric::Latency)?;
    }

    Ok(())
}

fn write_row(output: &mut impl Write, row: &Row, metric: Metric) -> io::Result<()> {
    write_common_fields(output, row, metric)?;

    match metric {
        Metric::Throughput => write_throughput_fields(output, row),
        Metric::Latency => write_latency_fields(output, row),
    }
}

fn write_common_fields(output: &mut impl Write, row: &Row, metric: Metric) -> io::Result<()> {
    for field in [
        row.language.as_str(),
        row.primitive.as_str(),
        row.scenario.as_str(),
        metric_name(metric),
    ] {
        write_csv_field(output, field)?;
        output.write_all(b",")?;
    }

    write!(
        output,
        "{},{},{},{},{:.0},",
        row.threads, row.repeat, row.batch, row.ops, row.wall_ns,
    )
}

fn write_throughput_fields(output: &mut impl Write, row: &Row) -> io::Result<()> {
    writeln!(output, "{:.0},,,,,,", row.throughput)
}

fn write_latency_fields(output: &mut impl Write, row: &Row) -> io::Result<()> {
    writeln!(
        output,
        ",{:.3},{:.3},{:.3},{:.3},{:.3},{:.3}",
        row.lat_avg_ns,
        row.lat_p50_ns,
        row.lat_p95_ns,
        row.lat_p99_ns,
        row.lat_p999_ns,
        row.lat_max_ns,
    )
}

fn write_csv_field(output: &mut impl Write, field: &str) -> io::Result<()> {
    let requires_escaping = field
        .chars()
        .any(|character| matches!(character, ',' | '"' | '\r' | '\n'));

    if requires_escaping {
        write!(output, "\"{}\"", field.replace('"', "\"\""))
    } else {
        output.write_all(field.as_bytes())
    }
}

fn metric_name(metric: Metric) -> &'static str {
    match metric {
        Metric::Throughput => "throughput",
        Metric::Latency => "latency",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_row() -> Row {
        Row {
            language: "rust, nightly".to_owned(),
            primitive: "mutex".to_owned(),
            scenario: "lock \"and\" unlock".to_owned(),
            threads: 4,
            repeat: 2,
            batch: 10,
            ops: 80,
            wall_ns: 1_000.0,
            throughput: 80_000_000.0,
            lat_avg_ns: 12.5,
            lat_p50_ns: 10.0,
            lat_p95_ns: 20.0,
            lat_p99_ns: 21.0,
            lat_p999_ns: 22.0,
            lat_max_ns: 25.0,
            ..Default::default()
        }
    }

    #[test]
    fn writes_throughput_and_latency_rows_with_the_schema_columns() {
        let row = sample_row();

        let mut output = Vec::new();

        write_row(&mut output, &row, Metric::Throughput).unwrap();
        write_row(&mut output, &row, Metric::Latency).unwrap();

        let output = String::from_utf8(output).unwrap();
        let mut lines = output.lines();

        assert_eq!(
            lines.next().unwrap(),
            "\"rust, nightly\",mutex,\"lock \"\"and\"\" unlock\",throughput,4,2,10,80,1000,80000000,,,,,,"
        );

        assert_eq!(
            lines.next().unwrap(),
            "\"rust, nightly\",mutex,\"lock \"\"and\"\" unlock\",latency,4,2,10,80,1000,,12.500,10.000,20.000,21.000,22.000,25.000"
        );

        assert!(lines.next().is_none());
    }

    #[test]
    fn writes_header_for_empty_output() {
        let options = Options::default();
        let mut output = Vec::new();

        write_rows(&mut output, &options, &[], true).unwrap();

        assert_eq!(
            String::from_utf8(output).unwrap(),
            format!("{CSV_HEADER}\n")
        );
    }

    #[test]
    fn does_not_write_header_when_appending() {
        let options = Options::default();
        let mut output = Vec::new();

        write_rows(&mut output, &options, &[], false).unwrap();

        assert!(output.is_empty());
    }

    #[test]
    fn escapes_csv_fields() {
        let mut output = Vec::new();

        write_csv_field(&mut output, "plain").unwrap();
        output.write_all(b",").unwrap();

        write_csv_field(&mut output, "with,comma").unwrap();
        output.write_all(b",").unwrap();

        write_csv_field(&mut output, "with \"quotes\"").unwrap();
        output.write_all(b",").unwrap();

        write_csv_field(&mut output, "with\nnewline").unwrap();

        assert_eq!(
            String::from_utf8(output).unwrap(),
            "plain,\"with,comma\",\"with \"\"quotes\"\"\",\"with\nnewline\""
        );
    }
}