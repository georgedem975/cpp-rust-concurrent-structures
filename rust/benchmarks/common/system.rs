use std::sync::OnceLock;
use std::time::Instant;

pub fn now_ns() -> f64 {
    static ORIGIN: OnceLock<Instant> = OnceLock::new();

    ORIGIN.get_or_init(Instant::now).elapsed().as_nanos() as f64
}

pub fn terminate_now(code: i32) -> ! {
    #[cfg(windows)]
    unsafe {
        let process = win::GetCurrentProcess();
        if win::TerminateProcess(process, code as u32) == 0 {
            eprintln!("failed to terminate benchmark process: {}", win::GetLastError());
        }
    }

    std::process::exit(code);
}

#[cfg(windows)]
mod win {
    use std::ffi::c_void;

    #[link(name = "kernel32")]
    unsafe extern "system" {
        pub fn GetCurrentProcess() -> *mut c_void;
        pub fn GetLastError() -> u32;
        pub fn TerminateProcess(process: *mut c_void, exit_code: u32) -> i32;
    }
}