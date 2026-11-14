use std::sync::Arc;

use bench::{parse_options, run_grid, validate_producer_consumer_grid, WorkerContext, WorkerRole};
use crossbeam_queue::ArrayQueue;

const QUEUE_CAPACITY: usize = 1024;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();

    validate_producer_consumer_grid(&options.threads)?;

    let batch_size = options.batch;
    let queue = Arc::new(ArrayQueue::new(QUEUE_CAPACITY));

    run_grid(
        &options,
        "rust",
        "bounded_queue",
        "mixed_50_50",
        move |context: WorkerContext| {
            let queue = Arc::clone(&queue);
            let role = context.role();
            let context = context.clone();

            Box::new(move || {
                let mut completed_operations = 0;

                match role {
                    WorkerRole::Producer => {
                        for _ in 0..batch_size {
                            if context.should_stop() {
                                break;
                            }

                            loop {
                                match queue.push(1) {
                                    Ok(()) => {
                                        completed_operations += 1;
                                        break;
                                    }
                                    Err(_) if context.should_stop() => break,
                                    Err(_) => {}
                                }
                            }

                            if context.should_stop() {
                                break;
                            }
                        }
                    }
                    WorkerRole::Consumer => {
                        for _ in 0..batch_size {
                            if context.should_stop() {
                                break;
                            }

                            loop {
                                if queue.pop().is_some() {
                                    completed_operations += 1;
                                    break;
                                }

                                if context.should_stop() {
                                    break;
                                }
                            }

                            if context.should_stop() {
                                break;
                            }
                        }
                    }
                }

                context.report_operations(completed_operations);
            })
        },
    )?;

    Ok(())
}