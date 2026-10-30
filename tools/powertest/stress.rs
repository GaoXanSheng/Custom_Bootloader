use std::thread;
use std::time::{Duration, Instant};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let secs: u64 = args.get(1).and_then(|s| s.parse().ok()).unwrap_or(60);
    let n = args
        .get(2)
        .and_then(|s| s.parse().ok())
        .unwrap_or_else(|| thread::available_parallelism().map(|v| v.get()).unwrap_or(8));
    let deadline = Instant::now() + Duration::from_secs(secs);
    let mut handles = Vec::with_capacity(n);
    for _ in 0..n {
        handles.push(thread::spawn(move || {
            let mut a = [1.0f64; 16];
            let b = [1.000001f64; 16];
            let mut acc = 0.0f64;
            while Instant::now() < deadline {
                for i in 0..16 {
                    a[i] = a[i] * b[i] + 0.0000001;
                    acc += a[i];
                }
                if acc > 1.0e6 {
                    acc = 0.0;
                    a = [1.0f64; 16];
                }
            }
            acc
        }));
    }
    for h in handles {
        let _ = h.join();
    }
}
