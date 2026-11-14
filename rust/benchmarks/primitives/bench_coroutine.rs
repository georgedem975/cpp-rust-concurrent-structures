use std::cell::RefCell;
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll, Waker};

use bench::{parse_options, run_grid};

struct YieldNow(bool);

impl Future for YieldNow {
    type Output = ();

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        let this = self.get_mut();
        if this.0 {
            Poll::Ready(())
        } else {
            this.0 = true;
            cx.waker().wake_by_ref();
            Poll::Pending
        }
    }
}

async fn coop_loop() {
    loop {
        YieldNow(false).await;
    }
}

thread_local! {
    static FUTURE: RefCell<Option<Pin<Box<dyn Future<Output = ()>>>>> = const { RefCell::new(None) };
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;

    run_grid(
        &options,
        "rust",
        "coroutine",
        "await_switch",
        move |_context| {
            Box::new(move || {
                FUTURE.with(|slot| {
                    let mut slot = slot.borrow_mut();
                    let future = slot.get_or_insert_with(|| Box::pin(coop_loop()));
                    let mut cx = Context::from_waker(Waker::noop());
                    for _ in 0..batch_size {
                        let _ = future.as_mut().poll(&mut cx);
                    }
                });
            })
        },
    )?;
    Ok(())
}