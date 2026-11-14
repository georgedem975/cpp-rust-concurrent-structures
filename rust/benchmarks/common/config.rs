use std::collections::HashSet;
use std::path::PathBuf;
use std::process;

const DEFAULT_THREADS: [usize; 5] = [2, 4, 8, 16, 32];
const DEFAULT_DURATION: f64 = 1.0;
const DEFAULT_REPEATS: usize = 10;
const DEFAULT_BATCH: u64 = 100;
const DEFAULT_WARMUP: f64 = 1.0;

#[derive(Clone, Debug)]
pub struct Options {
    pub threads: Vec<usize>,
    pub duration: f64,
    pub repeats: usize,
    pub batch: u64,
    pub warmup: f64,
    pub out: Option<PathBuf>,
    pub throughput: bool,
    pub latency: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            threads: DEFAULT_THREADS.to_vec(),
            duration: DEFAULT_DURATION,
            repeats: DEFAULT_REPEATS,
            batch: DEFAULT_BATCH,
            warmup: DEFAULT_WARMUP,
            out: None,
            throughput: false,
            latency: false,
        }
    }
}

pub fn parse_options() -> Options {
    parse_options_from(std::env::args().skip(1)).unwrap_or_else(|error| {
        eprintln!("bench: {error}");
        eprintln!(
            "usage: --threads=1,2,4 --duration=1.0 --repeats=10 \
             --batch=100 --warmup=1.0 --out=FILE [--throughput] [--latency]"
        );
        process::exit(2);
    })
}

fn parse_options_from(args: impl IntoIterator<Item = String>) -> Result<Options, String> {
    let mut options = Options::default();
    let mut args = args.into_iter();

    while let Some(argument) = args.next() {
        if argument == "--throughput" {
            options.throughput = true;
            continue;
        }

        if argument == "--latency" {
            options.latency = true;
            continue;
        }

        let (key, value) = match argument.split_once('=') {
            Some((key, value)) => (key, value.to_owned()),
            None => {
                let value = args
                    .next()
                    .ok_or_else(|| format!("missing value for {argument}"))?;

                (argument.as_str(), value)
            }
        };

        match key {
            "--threads" => {
                options.threads = parse_threads(&value)?;
            }
            "--duration" => {
                options.duration = parse_float("--duration", &value)?;

                if options.duration <= 0.0 {
                    return Err("--duration must be positive".to_string());
                }
            }
            "--repeats" => {
                options.repeats = parse_integer("--repeats", &value)?;

                if options.repeats == 0 {
                    return Err("--repeats must be positive".to_string());
                }
            }
            "--batch" => {
                options.batch = parse_integer("--batch", &value)?;

                if options.batch == 0 {
                    return Err("--batch must be positive".to_string());
                }
            }
            "--warmup" => {
                options.warmup = parse_float("--warmup", &value)?;

                if options.warmup < 0.0 {
                    return Err("--warmup cannot be negative".to_string());
                }
            }
            "--out" => {
                options.out = if value.is_empty() {
                    None
                } else {
                    Some(PathBuf::from(value))
                };
            }
            _ => return Err(format!("unknown option: {key}")),
        }
    }

    Ok(options)
}

fn parse_threads(value: &str) -> Result<Vec<usize>, String> {
    let mut threads = Vec::new();
    let mut unique = HashSet::new();

    for item in value.split(',') {
        let item = item.trim();

        if item.is_empty() {
            return Err("thread counts cannot be empty".to_string());
        }

        let count = parse_integer("--threads", item)?;

        if count == 0 {
            return Err("thread counts must be positive".to_string());
        }

        if !unique.insert(count) {
            return Err(format!("duplicate thread count: {count}"));
        }

        threads.push(count);
    }

    if threads.is_empty() {
        return Err("at least one thread count is required".to_string());
    }

    Ok(threads)
}

fn parse_integer<T>(name: &str, value: &str) -> Result<T, String>
where
    T: std::str::FromStr,
{
    value
        .parse()
        .map_err(|_| format!("invalid value for {name}: {value}"))
}

fn parse_float(name: &str, value: &str) -> Result<f64, String> {
    let parsed = value
        .parse::<f64>()
        .map_err(|_| format!("invalid value for {name}: {value}"))?;

    if !parsed.is_finite() {
        return Err(format!("{name} must be finite"));
    }

    Ok(parsed)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(args: &[&str]) -> Result<Options, String> {
        parse_options_from(args.iter().map(|argument| (*argument).to_owned()))
    }

    #[test]
    fn uses_the_documented_defaults() {
        let options = parse(&[]).unwrap();

        assert_eq!(options.threads, [2, 4, 8, 16, 32]);
        assert_eq!(options.duration, 1.0);
        assert_eq!(options.repeats, 10);
        assert_eq!(options.batch, 100);
        assert_eq!(options.warmup, 1.0);
        assert!(options.out.is_none());
        assert!(!options.throughput);
        assert!(!options.latency);
    }

    #[test]
    fn parses_benchmark_options_and_metrics() {
        let options = parse(&[
            "--threads=2,4",
            "--duration",
            "0.5",
            "--repeats=3",
            "--batch=16",
            "--warmup=0",
            "--out=results.csv",
            "--throughput",
            "--latency",
        ])
        .unwrap();

        assert_eq!(options.threads, [2, 4]);
        assert_eq!(options.duration, 0.5);
        assert_eq!(options.repeats, 3);
        assert_eq!(options.batch, 16);
        assert_eq!(options.warmup, 0.0);
        assert_eq!(
            options.out.as_deref(),
            Some(PathBuf::from("results.csv").as_path())
        );
        assert!(options.throughput);
        assert!(options.latency);
    }

    #[test]
    fn rejects_invalid_thread_counts() {
        assert!(parse(&["--threads=0"]).is_err());
        assert!(parse(&["--threads=2,0"]).is_err());
        assert!(parse(&["--threads=2,,4"]).is_err());
        assert!(parse(&["--threads=2,2"]).is_err());
        assert!(parse(&["--threads="]).is_err());
    }

    #[test]
    fn rejects_invalid_durations() {
        assert!(parse(&["--duration=0"]).is_err());
        assert!(parse(&["--duration=-1"]).is_err());
        assert!(parse(&["--duration=NaN"]).is_err());
        assert!(parse(&["--duration=inf"]).is_err());
    }

    #[test]
    fn rejects_invalid_repeats_and_batch() {
        assert!(parse(&["--repeats=0"]).is_err());
        assert!(parse(&["--batch=0"]).is_err());
    }

    #[test]
    fn rejects_negative_warmup() {
        assert!(parse(&["--warmup=-1"]).is_err());
        assert!(parse(&["--warmup=NaN"]).is_err());
    }

    #[test]
    fn rejects_unknown_options_and_missing_values() {
        assert!(parse(&["--unknown"]).is_err());
        assert!(parse(&["--duration"]).is_err());
    }
}