use std::sync::{Arc, RwLock};

use bench::{parse_options, run_grid, Options};

const WRITE_PERIOD_90_10: u64 = 10;
const WRITE_PERIOD_50_50: u64 = 2;

fn run_mixed(
    options: &Options,
    lock: Arc<RwLock<()>>,
    scenario: &'static str,
    write_period: u64,
) -> std::io::Result<()> {
    let batch_size = options.batch;
    run_grid(options, "rust", "rwlock", scenario, move |_| {
        let lock = Arc::clone(&lock);
        Box::new(move || {
            for operation in 1..=batch_size {
                if operation % write_period == 0 {
                    let _guard = lock.write().expect("rwlock poisoned");
                } else {
                    let _guard = lock.read().expect("rwlock poisoned");
                }
            }
        })
    })
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;
    let lock = Arc::new(RwLock::new(()));

    let read_lock = Arc::clone(&lock);
    run_grid(&options, "rust", "rwlock", "read", move |_| {
        let lock = Arc::clone(&read_lock);
        Box::new(move || {
            for _ in 0..batch_size {
                let _guard = lock.read().expect("rwlock poisoned");
            }
        })
    })?;

    let write_lock = Arc::clone(&lock);
    run_grid(&options, "rust", "rwlock", "write", move |_| {
        let lock = Arc::clone(&write_lock);
        Box::new(move || {
            for _ in 0..batch_size {
                let _guard = lock.write().expect("rwlock poisoned");
            }
        })
    })?;

    run_mixed(&options, Arc::clone(&lock), "mix_90_10", WRITE_PERIOD_90_10)?;
    run_mixed(&options, lock, "mix_50_50", WRITE_PERIOD_50_50)?;
    Ok(())
}