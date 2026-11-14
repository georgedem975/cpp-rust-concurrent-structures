use std::sync::atomic::{AtomicBool, AtomicI64, Ordering};
use std::sync::Arc;

use bench::{parse_options, run_grid};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;
    let flag = Arc::new(AtomicBool::new(false));

    let spinlock_flag = Arc::clone(&flag);
    run_grid(&options, "rust", "atomic_flag", "spinlock", move |_| {
        let flag = Arc::clone(&spinlock_flag);
        Box::new(move || {
            for _ in 0..batch_size {
                while flag.swap(true, Ordering::Acquire) {
                    std::hint::spin_loop();
                }
                flag.store(false, Ordering::Release);
            }
        })
    })?;

    let read_flag = Arc::clone(&flag);
    let read_sink = Arc::new(AtomicI64::new(0));
    run_grid(&options, "rust", "atomic_flag", "read", move |_| {
        let flag = Arc::clone(&read_flag);
        let sink = Arc::clone(&read_sink);
        Box::new(move || {
            let mut total = 0;
            for _ in 0..batch_size {
                total += i64::from(flag.load(Ordering::Relaxed));
            }
            sink.fetch_add(total, Ordering::Relaxed);
        })
    })?;

    Ok(())
}