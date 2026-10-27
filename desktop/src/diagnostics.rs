//! In-memory incident ledger. Repeated diagnostics are occurrences, not events.
//! Distinct sources/causes stay distinct; successful recovery ends only its source.
use std::collections::VecDeque;
use std::hash::{Hash, Hasher};
use std::time::{Duration, Instant};

const MAX_ROWS: usize = 256;
const MAX_INCIDENTS: usize = 128;

/// reqwest's outer Display omits its DNS/TLS/socket cause. Preserve that
/// distinction before incident comparison, with bounded depth and text.
pub fn describe_error(error: impl std::error::Error) -> String {
    let mut details = vec![error.to_string().chars().take(2048).collect::<String>()];
    let mut source = error.source();
    for _ in 0..8 {
        let Some(cause) = source else { break };
        let detail: String = cause.to_string().chars().take(1024).collect();
        if !detail.is_empty() && !details.contains(&detail) {
            details.push(detail);
        }
        source = cause.source();
    }
    details.join(": ")
}

pub struct Row {
    pub kind: String,
    pub message: String,
    pub occurrences: u64,
    pub active: bool,
    pub last_seen: Instant,
    id: u64,
}
struct Incident {
    source: String,
    cause: String,
    fingerprint: u64,
    row: u64,
}
#[derive(Default)]
pub struct EventLog {
    rows: VecDeque<Row>,
    incidents: VecDeque<Incident>,
    next_id: u64,
}
impl EventLog {
    pub fn len(&self) -> usize {
        self.rows.len()
    }
    pub fn active_count(&self) -> usize {
        self.incidents.len()
    }
    pub fn repeated_count(&self) -> u64 {
        self.rows.iter().fold(0u64, |sum, row| {
            sum.saturating_add(row.occurrences.saturating_sub(1))
        })
    }
    pub fn iter(&self) -> impl DoubleEndedIterator<Item = &Row> {
        self.rows.iter()
    }
    pub fn clear(&mut self) {
        self.rows.clear();
        self.incidents.clear();
    }
    pub fn record(&mut self, kind: &str, message: &str) {
        self.append(kind, message, false);
    }
    fn append(&mut self, kind: &str, message: &str, active: bool) -> u64 {
        if self.rows.len() == MAX_ROWS {
            if let Some(old) = self.rows.pop_front() {
                self.incidents.retain(|incident| incident.row != old.id);
            }
        }
        self.next_id = self.next_id.wrapping_add(1);
        self.rows.push_back(Row {
            kind: kind.into(),
            message: message.chars().take(2048).collect(),
            occurrences: 1,
            active,
            last_seen: Instant::now(),
            id: self.next_id,
        });
        self.next_id
    }
    /// No timer expiry: interleaved or long-lived failures cannot flood the log.
    /// The exact bounded cause, not a generic category, is the deduplication key.
    pub fn failure(&mut self, source: &str, cause: &str) {
        // Retain a bounded diagnostic key, but don't merge errors that differ
        // only beyond the displayed prefix. This hash is not an integrity API.
        let mut hasher = std::collections::hash_map::DefaultHasher::new();
        cause.hash(&mut hasher);
        let fingerprint = hasher.finish();
        let cause: String = cause.chars().take(2048).collect();
        if let Some(incident) = self
            .incidents
            .iter()
            .find(|i| i.source == source && i.cause == cause && i.fingerprint == fingerprint)
        {
            if let Some(row) = self.rows.iter_mut().find(|r| r.id == incident.row) {
                row.occurrences = row.occurrences.saturating_add(1);
                row.last_seen = Instant::now();
                return;
            }
        }
        if self.incidents.len() == MAX_INCIDENTS {
            if let Some(old) = self.incidents.pop_front() {
                if let Some(row) = self.rows.iter_mut().find(|r| r.id == old.row) {
                    row.active = false;
                }
            }
        }
        let row = self.append("Error", &format!("{source}: {cause}"), true);
        self.incidents.push_back(Incident {
            source: source.into(),
            cause,
            fingerprint,
            row,
        });
    }
    pub fn recover(&mut self, source: &str) {
        let mut count = 0u64;
        for incident in self.incidents.iter().filter(|i| i.source == source) {
            if let Some(row) = self.rows.iter_mut().find(|r| r.id == incident.row) {
                row.active = false;
                count = count.saturating_add(row.occurrences);
            }
        }
        self.incidents.retain(|i| i.source != source);
        if count != 0 {
            self.record(
                "Recovery",
                &format!("{source} recovered after {count} diagnostic occurrences."),
            );
        }
    }
    /// Changing peer/auth/port is not proof the old peer recovered.
    pub fn reset_sources(&mut self, prefix: &str) {
        for incident in self
            .incidents
            .iter()
            .filter(|i| i.source.starts_with(prefix))
        {
            if let Some(row) = self.rows.iter_mut().find(|r| r.id == incident.row) {
                row.active = false;
            }
        }
        self.incidents.retain(|i| !i.source.starts_with(prefix));
    }
}

/// Read-only retry cadence; never retries or replays a device mutation.
pub struct Backoff {
    initial: Duration,
    maximum: Duration,
    current: Duration,
}
impl Backoff {
    pub fn new(initial: Duration, maximum: Duration) -> Self {
        Self {
            initial,
            maximum,
            current: initial,
        }
    }
    pub fn delay(&self) -> Duration {
        self.current
    }
    pub fn failed(&mut self) {
        self.current = self.current.saturating_mul(2).min(self.maximum);
    }
    pub fn reset(&mut self) {
        self.current = self.initial;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn generic_outer_errors_keep_distinct_underlying_causes() {
        #[derive(Debug)]
        struct Outer(std::io::Error);
        impl std::fmt::Display for Outer {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                f.write_str("request failed")
            }
        }
        impl std::error::Error for Outer {
            fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
                Some(&self.0)
            }
        }
        let dns = describe_error(Outer(std::io::Error::new(
            std::io::ErrorKind::AddrNotAvailable,
            "DNS failed",
        )));
        let refused = describe_error(Outer(std::io::Error::new(
            std::io::ErrorKind::ConnectionRefused,
            "connection refused",
        )));
        assert!(dns.contains("DNS failed"));
        assert!(refused.contains("connection refused"));
        let mut log = EventLog::default();
        log.failure("HTTP", &dns);
        log.failure("HTTP", &refused);
        log.failure("HTTP", &dns);
        assert_eq!(log.len(), 2);
        assert_eq!(log.repeated_count(), 1);
    }
    #[test]
    fn interleaved_failures_are_counted_but_distinct_causes_and_sources_survive() {
        let mut log = EventLog::default();
        for _ in 0..1000 {
            log.failure("HTTP status", "DNS failed");
            log.failure("WebSocket", "DNS failed");
        }
        log.failure("HTTP status", "401 unauthorized");
        log.failure("HTTP status", "DNS failed");
        assert_eq!(log.len(), 3);
        assert_eq!(log.rows[0].occurrences, 1001);
        assert_eq!(log.rows[1].occurrences, 1000);
        assert_eq!(log.rows[2].occurrences, 1);
    }
    #[test]
    fn recovery_is_source_specific_and_a_new_outage_is_visible() {
        let mut log = EventLog::default();
        log.failure("HTTP status", "offline");
        log.failure("WebSocket", "offline");
        log.recover("HTTP status");
        log.recover("HTTP status");
        assert_eq!(log.len(), 3);
        assert!(!log.rows[0].active && log.rows[1].active);
        log.failure("HTTP status", "offline");
        assert_eq!(log.len(), 4);
        assert!(log.rows[3].active);
        log.reset_sources("WebSocket");
        assert_eq!(log.len(), 4); // Changing peer is not a recovery event.
        assert!(!log.rows[1].active);
    }
    #[test]
    fn explicit_operations_are_not_suppressed_and_clear_rearms_diagnostics() {
        let mut log = EventLog::default();
        log.record("Operation failed", "rejected");
        log.record("Operation failed", "rejected");
        assert_eq!(log.len(), 2);
        log.failure("HTTP", "offline");
        log.clear();
        log.failure("HTTP", "offline");
        assert_eq!(log.len(), 1);
        assert_eq!(log.rows[0].occurrences, 1);
    }
    #[test]
    fn history_incidents_and_retry_delay_are_bounded() {
        let mut log = EventLog::default();
        for n in 0..1000 {
            log.failure("HTTP", &format!("cause {n}"));
        }
        assert_eq!(log.len(), MAX_ROWS);
        assert_eq!(log.incidents.len(), MAX_INCIDENTS);
        log.recover("HTTP");
        assert!(log.incidents.is_empty());
        let mut backoff = Backoff::new(Duration::from_secs(2), Duration::from_secs(30));
        for _ in 0..100 {
            backoff.failed();
        }
        assert_eq!(backoff.delay(), Duration::from_secs(30));
        backoff.reset();
        assert_eq!(backoff.delay(), Duration::from_secs(2));
    }
    #[test]
    fn different_long_error_tails_do_not_merge() {
        let mut log = EventLog::default();
        let prefix = "x".repeat(2048);
        log.failure("HTTP", &format!("{prefix}a"));
        log.failure("HTTP", &format!("{prefix}b"));
        log.failure("HTTP", &format!("{prefix}a"));
        assert_eq!(log.len(), 2);
        assert_eq!(log.repeated_count(), 1);
        assert_eq!(log.active_count(), 2);
    }
}
