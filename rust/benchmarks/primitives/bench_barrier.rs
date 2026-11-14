use std::sync::Barrier;

use bench::{emit, now_ns, parse_options, Row};

const CALIBRATION_ROUNDS: usize = 32;
const MAX_ROUNDS: usize = 10_000_000;

fn run_rounds(batch_size: u64, threads: usize, repeat: usize, rounds: usize) -> Row {
    let phase_barrier = Barrier::new(threads);
    let start_gate = Barrier::new(threads + 1);

    let (wall_ns, samples_by_thread) = std::thread::scope(|scope| {
        let workers: Vec<_> = (0..threads)
            .map(|_| {
                let phase_barrier = &phase_barrier;
                let start_gate = &start_gate;
                scope.spawn(move || {
                    let mut samples = Vec::with_capacity(rounds);
                    start_gate.wait();

                    for _ in 0..rounds {
                        let batch_started_at = now_ns();
                        for _ in 0..batch_size {
                            phase_barrier.wait();
                        }
                        samples.push(now_ns() - batch_started_at);
                    }

                    samples
                })
            })
            .collect();

        let started_at = now_ns();
        start_gate.wait();
        let samples: Vec<Vec<f64>> = workers
            .into_iter()
            .map(|worker| worker.join().expect("barrier worker panicked"))
            .collect();

        (now_ns() - started_at, samples)
    });

    let mut row = Row {
        language: "rust".to_owned(),
        primitive: "barrier".to_owned(),
        scenario: "arrive_and_wait".to_owned(),
        threads,
        repeat,
        batch: batch_size,
        ops: threads as u64 * rounds as u64 * batch_size,
        wall_ns,
        ..Default::default()
    };
    row.calculate_throughput();

    let mut latency_samples: Vec<_> = samples_by_thread.into_iter().flatten().collect();
    row.calculate_latency(&mut latency_samples);
    row
}

fn calibrate_rounds(batch_size: u64, threads: usize, target_seconds: f64) -> usize {
    let probe = run_rounds(batch_size, threads, usize::MAX, CALIBRATION_ROUNDS);
    if probe.wall_ns <= 0.0 {
        return 1;
    }

    let average_round_ns = probe.wall_ns / CALIBRATION_ROUNDS as f64;
    let estimated_rounds = (target_seconds * 1e9 / average_round_ns).ceil();
    estimated_rounds.clamp(1.0, MAX_ROUNDS as f64) as usize
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();

    if options.warmup > 0.0 {
        if let Some(&threads) = options.threads.last() {
            let rounds = calibrate_rounds(options.batch, threads, options.warmup);
            let _ = run_rounds(options.batch, threads, usize::MAX, rounds);
        }
    }

    let mut rows = Vec::new();
    for repeat in 0..options.repeats {
        for &threads in &options.threads {
            let rounds = calibrate_rounds(options.batch, threads, options.duration);
            rows.push(run_rounds(options.batch, threads, repeat, rounds));
        }
    }

    emit(&options, &rows)?;
    Ok(())
}