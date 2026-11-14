use std::sync::Arc;

use bench::{
    parse_options, run_grid, terminate_now, validate_producer_consumer_grid, WorkerContext,
    WorkerRole,
};
use scc::Stack;

const INITIAL_ITEMS: usize = 1024;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();

    validate_producer_consumer_grid(&options.threads)?;

    let batch_size = options.batch;
    let stack: Arc<Stack<i32>> = Arc::new(Stack::default());

    for _ in 0..INITIAL_ITEMS {
        stack.push(0);
    }

    let stack_for_workers = Arc::clone(&stack);

    run_grid(
        &options,
        "rust",
        "scc_stack",
        "mixed_50_50",
        move |context: WorkerContext| {
            let stack = Arc::clone(&stack_for_workers);
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

                            stack.push(1);
                            completed_operations += 1;
                        }
                    }
                    WorkerRole::Consumer => {
                        for operation in 0..batch_size {
                            if operation % 16 == 0 && context.should_stop() {
                                break;
                            }

                            loop {
                                if stack.pop().is_some() {
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