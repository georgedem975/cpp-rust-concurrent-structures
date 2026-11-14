use bench::{emit, now_ns, parse_options, Row};

fn spawn_and_join(thread_count: usize) -> (f64, Vec<f64>) {
    let started_at = now_ns();

    let spawn_samples = std::thread::scope(|scope| {
        let mut workers = Vec::with_capacity(thread_count);
        let mut spawn_samples = Vec::with_capacity(thread_count);

        for _ in 0..thread_count {
            let spawn_started_at = now_ns();
            workers.push(scope.spawn(|| {}));
            spawn_samples.push(now_ns() - spawn_started_at);
        }

        for worker in workers {
            worker.join().expect("spawned worker panicked");
        }

        spawn_samples
    });

    (now_ns() - started_at, spawn_samples)
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let mut rows = Vec::new();

    for repeat in 0..options.repeats {
        for &threads in &options.threads {
            let (wall_ns, mut latency_samples) = spawn_and_join(threads);
            let mut row = Row {
                language: "rust".to_owned(),
                primitive: "thread".to_owned(),
                scenario: "spawn_join".to_owned(),
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