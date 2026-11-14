use std::sync::atomic::{AtomicI64, AtomicPtr, Ordering};
use std::sync::Arc;

use bench::{parse_options, run_grid};

struct Cell {
    value: i32,
}

static CELL_A: Cell = Cell { value: 0 };
static CELL_B: Cell = Cell { value: 0 };

fn cell_a() -> *mut Cell {
    std::ptr::addr_of!(CELL_A).cast_mut()
}

fn cell_b() -> *mut Cell {
    std::ptr::addr_of!(CELL_B).cast_mut()
}

#[inline]
fn other_cell(pointer: *mut Cell) -> *mut Cell {
    if pointer == cell_a() {
        cell_b()
    } else {
        cell_a()
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;
    let pointer = Arc::new(AtomicPtr::new(cell_a()));
    let read_sink = Arc::new(AtomicI64::new(0));

    let load_pointer = Arc::clone(&pointer);
    let load_sink = Arc::clone(&read_sink);
    run_grid(&options, "rust", "atomic_ptr", "load", move |_| {
        let pointer = Arc::clone(&load_pointer);
        let sink = Arc::clone(&load_sink);
        Box::new(move || {
            let mut total = 0;
            for _ in 0..batch_size {
                let cell = pointer.load(Ordering::Acquire);
                unsafe {
                    total += (*cell).value as i64;
                }
            }
            sink.fetch_add(total, Ordering::Relaxed);
        })
    })?;

    let cas_pointer = Arc::clone(&pointer);
    run_grid(&options, "rust", "atomic_ptr", "cas_loop", move |_| {
        let pointer = Arc::clone(&cas_pointer);
        Box::new(move || {
            for _ in 0..batch_size {
                let mut expected = pointer.load(Ordering::Relaxed);
                loop {
                    let desired = other_cell(expected);
                    match pointer.compare_exchange_weak(
                        expected,
                        desired,
                        Ordering::AcqRel,
                        Ordering::Acquire,
                    ) {
                        Ok(_) => break,
                        Err(actual) => expected = actual,
                    }
                }
            }
        })
    })?;

    Ok(())
}