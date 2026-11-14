use std::collections::VecDeque;
use std::sync::{Barrier, Condvar, Mutex};

use bench::{emit, now_ns, parse_options, Row};

const QUEUE_CAPACITY: usize = 64;
const NANOS_PER_SECOND: f64 = 1_000_000_000.0;

struct QueueState {
    items: VecDeque<i32>,
    finished: bool,
}

struct BoundedQueue {
    state: Mutex<QueueState>,
    not_empty: Condvar,
    not_full: Condvar,
}

struct WorkerResult {
    latency_samples: Vec<f64>,
    operations: u64,
}

impl BoundedQueue {
    fn new() -> Self {
        Self {
            state: Mutex::new(QueueState {
                items: VecDeque::with_capacity(QUEUE_CAPACITY),
                finished: false,
            }),
            not_empty: Condvar::new(),
            not_full: Condvar::new(),
        }
    }

    fn push(&self, value: i32) {
        let mut state = self.state.lock().expect("queue mutex poisoned");
        while state.items.len() >= QUEUE_CAPACITY && !state.finished {
            state = self
                .not_full
                .wait(state)
                .expect("queue mutex poisoned while pushing");
        }

        if state.finished {
            return;
        }

        state.items.push_back(value);
        drop(state);
        self.not_empty.notify_one();
    }

    fn pop(&self) -> Option<i32> {
        let mut state = self.state.lock().expect("queue mutex poisoned");
        while state.items.is_empty() && !state.finished {
            state = self
                .not_empty
                .wait(state)
                .expect("queue mutex poisoned while popping");
        }

        let value = state.items.pop_front();
        if value.is_some() {
            drop(state);
            self.not_full.notify_one();
        }
        value
    }

    fn finish(&self) {
        let mut state = self.state.lock().expect("queue mutex poisoned");
        state.finished = true;
        drop(state);
        self.not_empty.notify_all();
    }
}

fn run_handoff(duration_seconds: f64, threads: usize) -> (f64, Vec<WorkerResult>) {
    let queue = BoundedQueue::new();
    let start_gate = Barrier::new(threads + 1);
    let started_at = now_ns();
    let deadline = started_at + duration_seconds * NANOS_PER_SECOND;

    let (wall_ns, results) = std::thread::scope(|scope| {
        let workers: Vec<_> = (0..threads)
            .map(|thread_id| {
                let queue = &queue;
                let start_gate = &start_gate;
                scope.spawn(move || {
                    let mut result = WorkerResult {
                        latency_samples: Vec::new(),
                        operations: 0,
                    };
                    start_gate.wait();

                    if threads == 1 {
                        run_single_thread(queue, deadline, &mut result);
                    } else if thread_id == 0 {
                        run_producer(queue, deadline, &mut result);
                    } else {
                        run_consumer(queue, &mut result);
                    }

                    result
                })
            })
            .collect();

        start_gate.wait();
        let results = workers
            .into_iter()
            .map(|worker| worker.join().expect("condvar worker panicked"))
            .collect();

        (now_ns() - started_at, results)
    });

    (wall_ns, results)
}

fn run_producer(queue: &BoundedQueue, deadline: f64, result: &mut WorkerResult) {
    let mut value = 1;
    while now_ns() < deadline {
        let started_at = now_ns();
        queue.push(value);
        result.latency_samples.push(now_ns() - started_at);
        result.operations += 1;
        value = value.wrapping_add(1);
    }

    queue.finish();
}

fn run_consumer(queue: &BoundedQueue, result: &mut WorkerResult) {
    loop {
        let started_at = now_ns();
        match queue.pop() {
            Some(value) => {
                result.latency_samples.push(now_ns() - started_at);
                result.operations += 1;
                std::hint::black_box(value);
            }
            None => break,
        }
    }
}

fn run_single_thread(queue: &BoundedQueue, deadline: f64, result: &mut WorkerResult) {
    while now_ns() < deadline {
        let started_at = now_ns();
        queue.push(1);
        result.latency_samples.push(now_ns() - started_at);
        result.operations += 1;

        let started_at = now_ns();
        if let Some(value) = queue.pop() {
            result.latency_samples.push(now_ns() - started_at);
            result.operations += 1;
            std::hint::black_box(value);
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let mut rows = Vec::new();

    for repeat in 0..options.repeats {
        for &threads in &options.threads {
            let (wall_ns, worker_results) = run_handoff(options.duration, threads);
            let operations = worker_results.iter().map(|result| result.operations).sum();
            let mut latency_samples: Vec<_> = worker_results
                .into_iter()
                .flat_map(|result| result.latency_samples)
                .collect();

            let mut row = Row {
                language: "rust".to_owned(),
                primitive: "condvar".to_owned(),
                scenario: "handoff".to_owned(),
                threads,
                repeat,
                batch: 1,
                ops: operations,
                wall_ns,
                ..Default::default()
            };
            row.calculate_throughput();
            row.calculate_latency(&mut latency_samples);
            rows.push(row);
        }
    }

    emit(&options, &rows)?;
    Ok(())
}