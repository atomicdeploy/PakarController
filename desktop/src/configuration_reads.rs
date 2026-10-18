//! Optional settings reads never flood the control queue at connection time.
use std::time::{Duration, Instant};
const PATHS: [&str; 3] = ["/api/network", "/api/device/name", "/api/cloud"];
pub struct Queue { next: usize, pending: Option<Instant>, retry: Instant }
impl Default for Queue {
    fn default() -> Self { Self { next: 0, pending: None, retry: Instant::now() } }
}
impl Queue {
    pub fn reset(&mut self) { *self = Self::default(); }
    pub fn take_due(&mut self) -> Option<&'static str> {
        if self.pending.is_some_and(|at| at.elapsed() >= Duration::from_secs(24)) {
            self.pending = None;
            self.retry = Instant::now() + Duration::from_secs(2);
        }
        if self.pending.is_some() || Instant::now() < self.retry { return None; }
        let path = *PATHS.get(self.next)?;
        self.pending = Some(Instant::now());
        Some(path)
    }
    pub fn finish(&mut self, path: &str, success: bool) {
        if self.pending.is_none() || PATHS.get(self.next).copied() != Some(path) { return; }
        self.pending = None;
        if success { self.next += 1; }
        else { self.retry = Instant::now() + Duration::from_secs(2); }
    }
    pub fn failed_source(&mut self, source: &str) {
        if let Some(path) = source.strip_prefix("HTTP GET ") { self.finish(path, false); }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn optional_reads_are_serial_bounded_and_retry_only_the_failed_read() {
        let mut q = Queue::default();
        assert_eq!(q.take_due(), Some(PATHS[0]));
        assert_eq!(q.take_due(), None);
        q.finish(PATHS[1], true); // Unrelated/late result cannot release the slot.
        assert_eq!(q.take_due(), None);
        q.failed_source("HTTP GET /api/network");
        assert_eq!(q.take_due(), None);
        q.retry = Instant::now();
        assert_eq!(q.take_due(), Some(PATHS[0]));
        q.finish(PATHS[0], true);
        for path in &PATHS[1..] {
            assert_eq!(q.take_due(), Some(*path));
            assert_eq!(q.take_due(), None);
            q.finish(path, true);
        }
        assert_eq!(q.take_due(), None);
        q.reset();
        assert_eq!(q.take_due(), Some(PATHS[0]));
        q.pending = Some(Instant::now() - Duration::from_secs(25));
        assert_eq!(q.take_due(), None); // Expiry backs off instead of flooding.
    }
}
