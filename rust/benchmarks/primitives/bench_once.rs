use std::sync::{Arc, Barrier, Once};

use bench::{emit, now_ns, parse_options, run_grid, Row};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;

    let once = Arc::new(Once::new());
    once.call_once(|| {});

    let hot_path_once = Arc::clone(&once);
    run_grid(&options, "rust", "once", "hot_path", move |_| {
        let once = Arc::clone(&hot_path_once);
        Box::new(move || {
            for _ in 0..batch_size {
                once.call_once(|| {});
            }
        })
    })?;

    let mut rows = Vec::new();
    for repeat in 0..options.repeats {
        for &threads in &options.threads {
            let once = Once::new();
            let start_gate = Barrier::new(threads + 1);

            let (wall_ns, mut latency_samples): (f64, Vec<f64>) = std::thread::scope(|scope| {
                let workers: Vec<_> = (0..threads)
                    .map(|_| {
                        let once = &once;
                        let start_gate = &start_gate;
                        scope.spawn(move || {
                            start_gate.wait();
                            let started_at = now_ns();
                            once.call_once(|| {});
                            now_ns() - started_at
                        })
                    })
                    .collect();

                let started_at = now_ns();
                start_gate.wait();
                let samples = workers
                    .into_iter()
                    .map(|worker| worker.join().expect("once worker panicked"))
                    .collect();

                (now_ns() - started_at, samples)
            });

            let mut row = Row {
                language: "rust".to_owned(),
                primitive: "once".to_owned(),
                scenario: "cold_path".to_owned(),
                threads,
                repeat,
                batch: 1,
                ops: threads as u64,
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