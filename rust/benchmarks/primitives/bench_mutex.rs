use std::sync::{Arc, Mutex};

use bench::{parse_options, run_grid};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;
    let lock = Arc::new(Mutex::new(()));

    let lock_for_blocking = Arc::clone(&lock);
    run_grid(&options, "rust", "mutex", "lock_unlock", move |_| {
        let lock = Arc::clone(&lock_for_blocking);
        Box::new(move || {
            for _ in 0..batch_size {
                let _guard = lock.lock().expect("mutex poisoned");
            }
        })
    })?;

    let lock_for_try = Arc::clone(&lock);
    run_grid(&options, "rust", "mutex", "try_lock", move |_| {
        let lock = Arc::clone(&lock_for_try);
        Box::new(move || {
            for _ in 0..batch_size {
                let _guard = lock.try_lock();
            }
        })
    })?;

    Ok(())
}