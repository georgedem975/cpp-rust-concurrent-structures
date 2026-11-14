use std::io;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Barrier, OnceLock};
use std::time::{Duration, Instant};

use crate::{emit, Metric, Options, Row};

#[derive(Clone, Debug)]
pub struct WorkerContext {
    pub id: usize,
    pub total_threads: usize,
    control: Arc<WorkerControl>,
    completed_operations: Arc<AtomicU64>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum WorkerRole {
    Producer,
    Consumer,
}

impl WorkerContext {
    fn new(
        id: usize,
        total_threads: usize,
        control: Arc<WorkerControl>,
        completed_operations: Arc<AtomicU64>,
    ) -> Self {
        Self {
            id,
            total_threads,
            control,
            completed_operations,
        }
    }

    pub fn role(&self) -> WorkerRole {
        if self.id < self.total_threads / 2 {
            WorkerRole::Producer
        } else {
            WorkerRole::Consumer
        }
    }

    pub fn should_stop(&self) -> bool {
        self.control.should_stop()
    }

    pub fn report_operations(&self, operations: u64) {
        self.completed_operations
            .store(operations, Ordering::Relaxed);
    }
}

pub fn validate_producer_consumer_grid(threads: &[usize]) -> io::Result<()> {
    if threads.iter().any(|&count| count < 2 || count % 2 != 0) {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "producer/consumer benchmarks require even thread counts >= 2",
        ));
    }

    Ok(())
}

pub type Worker = dyn FnMut() + Send;

#[derive(Debug)]
struct WorkerControl {
    started_at: OnceLock<Instant>,
    duration: Duration,
    cancelled: AtomicBool,
}

impl WorkerControl {
    fn new(duration: Duration) -> Self {
        Self {
            started_at: OnceLock::new(),
            duration,
            cancelled: AtomicBool::new(false),
        }
    }

    fn start(&self, started_at: Instant) {
        self.started_at
            .set(started_at)
            .expect("benchmark start time must be initialized only once");
    }

    fn started_at(&self) -> Instant {
        *self
            .started_at
            .get()
            .expect("benchmark start time must be initialized")
    }

    fn should_stop(&self) -> bool {
        if Instant::now() >= self.started_at() + self.duration {
            self.cancelled.store(true, Ordering::Release);
        }

        self.cancelled.load(Ordering::Acquire)
    }
}

#[derive(Clone, Copy)]
struct Benchmark {
    language: &'static str,
    primitive: &'static str,
    scenario: &'static str,
}

#[derive(Clone, Copy)]
struct RunConfig {
    threads: usize,
    repeat: usize,
    duration: Duration,
    metric: Metric,
}

struct WorkerResult {
    latency_samples: Vec<f64>,
    operations: u64,
    finished_at: Instant,
}

fn run_worker(
    worker: &mut Worker,
    start_gate: &Barrier,
    context: WorkerContext,
    config: RunConfig,
    batch_size: u64,
) -> WorkerResult {
    let mut latency_samples = Vec::new();
    let mut operations = 0;

    start_gate.wait();

    loop {
        context
            .completed_operations
            .store(u64::MAX, Ordering::Relaxed);
        let batch_started_at = Instant::now();

        worker();

        let reported_operations = context
            .completed_operations
            .swap(u64::MAX, Ordering::Relaxed);
        let completed_operations = if reported_operations == u64::MAX {
            batch_size
        } else {
            reported_operations
        };

        if config.metric == Metric::Latency {
            if completed_operations > 0 {
                let batch_duration_ns = batch_started_at.elapsed().as_nanos() as f64;
                latency_samples.push(
                    batch_duration_ns * batch_size as f64 / completed_operations as f64,
                );
            }
        }

        operations += completed_operations;

        if context.should_stop() {
            break;
        }
    }

    WorkerResult {
        latency_samples,
        operations,
        finished_at: Instant::now(),
    }
}

fn run_point(
    options: &Options,
    benchmark: Benchmark,
    config: RunConfig,
    make_worker: &impl Fn(WorkerContext) -> Box<Worker>,
) -> io::Result<Row> {
    let start_gate = Barrier::new(config.threads + 1);
    let control = Arc::new(WorkerControl::new(config.duration));
    let batch_size = options.batch;

    let contexts: Vec<_> = (0..config.threads)
        .map(|id| {
            WorkerContext::new(
                id,
                config.threads,
                Arc::clone(&control),
                Arc::new(AtomicU64::new(u64::MAX)),
            )
        })
        .collect();
    let mut workers: Vec<_> = contexts
        .iter()
        .cloned()
        .map(make_worker)
        .collect();

    let (started_at, worker_results) = std::thread::scope(|scope| {
        let handles: Vec<_> = workers
            .iter_mut()
            .zip(contexts.iter())
            .map(|(worker, context)| {
                let start_gate = &start_gate;
                let context = context.clone();

                scope.spawn(move || run_worker(worker, start_gate, context, config, batch_size))
            })
            .collect();

        let started_at = Instant::now();
        control.start(started_at);

        start_gate.wait();

        let joined_results: Vec<_> = handles.into_iter().map(|handle| handle.join()).collect();

        let worker_results = joined_results
            .into_iter()
            .collect::<Result<Vec<_>, _>>()
            .map_err(|_| io::Error::other("benchmark worker panicked"))?;

        Ok::<_, io::Error>((started_at, worker_results))
    })?;

    let finished_at = worker_results
        .iter()
        .fold(started_at, |latest, result| latest.max(result.finished_at));

    let wall_ns = finished_at.duration_since(started_at).as_nanos() as f64;

    let operations = worker_results.iter().map(|result| result.operations).sum();

    let mut row = Row {
        language: benchmark.language.to_owned(),
        primitive: benchmark.primitive.to_owned(),
        scenario: benchmark.scenario.to_owned(),
        threads: config.threads,
        repeat: config.repeat,
        batch: batch_size,
        metric: Some(config.metric),
        ops: operations,
        wall_ns,
        ..Default::default()
    };

    match config.metric {
        Metric::Throughput => {
            row.calculate_throughput();
        }
        Metric::Latency => {
            let mut latency_samples: Vec<f64> = worker_results
                .into_iter()
                .flat_map(|result| result.latency_samples)
                .collect();

            row.calculate_latency(&mut latency_samples);
        }
    }

    Ok(row)
}

pub fn run_grid<F>(
    options: &Options,
    language: &'static str,
    primitive: &'static str,
    scenario: &'static str,
    make_worker: F,
) -> io::Result<()>
where
    F: Fn(WorkerContext) -> Box<Worker>,
{
    let metrics: Vec<_> = enabled_metrics(options).collect();

    if metrics.is_empty() {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "at least one metric must be enabled",
        ));
    }

    let benchmark = Benchmark {
        language,
        primitive,
        scenario,
    };

    if options.warmup > 0.0 {
        if let Some(&threads) = options.threads.last() {
            let warmup = RunConfig {
                threads,
                repeat: 0,
                duration: Duration::from_secs_f64(options.warmup),
                metric: Metric::Throughput,
            };

            for &metric in &metrics {
                run_point(
                    options,
                    benchmark,
                    RunConfig { metric, ..warmup },
                    &make_worker,
                )?;
            }
        }
    }

    let mut rows = Vec::with_capacity(options.repeats * options.threads.len() * metrics.len());

    for repeat in 0..options.repeats {
        for &threads in &options.threads {
            for &metric in &metrics {
                rows.push(run_point(
                    options,
                    benchmark,
                    RunConfig {
                        threads,
                        repeat,
                        duration: Duration::from_secs_f64(options.duration),
                        metric,
                    },
                    &make_worker,
                )?);
            }
        }
    }

    emit(options, &rows)
}

fn enabled_metrics(options: &Options) -> impl Iterator<Item = Metric> {
    [
        options.throughput.then_some(Metric::Throughput),
        options.latency.then_some(Metric::Latency),
    ]
    .into_iter()
    .flatten()
}

#[cfg(test)]
mod tests {
    use std::sync::atomic::AtomicU64;
    use std::sync::Arc;
    use std::time::Duration;

    use super::{
        validate_producer_consumer_grid, WorkerContext, WorkerControl, WorkerRole,
    };

    fn context(id: usize, total_threads: usize) -> WorkerContext {
        WorkerContext::new(
            id,
            total_threads,
            Arc::new(WorkerControl::new(Duration::from_secs(1))),
            Arc::new(AtomicU64::new(u64::MAX)),
        )
    }

    #[test]
    fn assigns_equal_producer_and_consumer_groups() {
        for total_threads in [2, 4, 8, 32] {
            let producer_count = total_threads / 2;
            for id in 0..total_threads {
                let expected_role = if id < producer_count {
                    WorkerRole::Producer
                } else {
                    WorkerRole::Consumer
                };
                assert_eq!(context(id, total_threads).role(), expected_role);
            }
        }
    }

    #[test]
    fn rejects_thread_grids_that_cannot_be_split_evenly() {
        assert!(validate_producer_consumer_grid(&[2, 4, 8]).is_ok());
        assert!(validate_producer_consumer_grid(&[1, 2]).is_err());
        assert!(validate_producer_consumer_grid(&[2, 3]).is_err());
    }
}