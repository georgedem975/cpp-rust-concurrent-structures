use std::sync::Arc;

use bench::{
    parse_options, run_grid, terminate_now, validate_producer_consumer_grid, WorkerContext,
    WorkerRole,
};
use scc::Queue;

const INITIAL_ITEMS: usize = 1024;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();

    validate_producer_consumer_grid(&options.threads)?;

    let batch_size = options.batch;
    let queue: Arc<Queue<i32>> = Arc::new(Queue::default());

    for _ in 0..INITIAL_ITEMS {
        let _ = queue.push(0);
    }

    let queue_for_workers = Arc::clone(&queue);

    run_grid(
        &options,
        "rust",
        "scc_queue",
        "mixed_50_50",
        move |context: WorkerContext| {
            let queue = Arc::clone(&queue_for_workers);
            let role = context.role();
            let context = context.clone();

            Box::new(move || {
                let mut completed_operations = 0;

                match role {
                    WorkerRole::Producer => {
                        for operation in 0..batch_size {
                            if operation % 16 == 0 && context.should_stop() {
                                break;
                            }

                            let _ = queue.push(1);
                            completed_operations += 1;
                        }
                    }
                    WorkerRole::Consumer => {
                        for operation in 0..batch_size {
                            if operation % 16 == 0 && context.should_stop() {
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

    terminate_now(0)
}