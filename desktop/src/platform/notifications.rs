//! Policy and bounded delivery, separate from Pealayer's shared in-app hub.
use crate::messaging::{Severity, ToastRequest};
use serde::{Deserialize, Serialize};
use std::collections::VecDeque;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
pub enum Policy {
    Off,
    #[default]
    Important,
    All,
}
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Preferences {
    pub in_app: bool,
    pub system: Policy,
    pub sound: bool,
}
impl Default for Preferences {
    fn default() -> Self {
        Self { in_app: true, system: Policy::Important, sound: false }
    }
}
#[derive(Default)]
pub struct Delivery {
    queue: VecDeque<ToastRequest>,
    recent: VecDeque<(String, u64)>,
    pub accepted: u64,
    pub suppressed: u64,
    pub failed: u64,
    pub last_result: String,
}
impl Delivery {
    pub fn enqueue(&mut self, request: ToastRequest) {
        if request.validate().is_err() { return; }
        // A fast control burst must not replace important pending alerts.
        if self.queue.len() == 32 {
            if let Some(index) = self.queue.iter().position(|r| !important(r)) {
                self.queue.remove(index);
            } else if !important(&request) {
                self.suppressed += 1;
                return;
            } else { self.queue.pop_front(); }
            self.suppressed += 1;
        }
        self.queue.push_back(request);
    }
    pub fn next(&mut self, preferences: &Preferences, now: u64) -> Option<ToastRequest> {
        while let Some(request) = self.queue.pop_front() {
            let id = request.id.as_deref().unwrap_or("message");
            let test = id == "pakar.notification-test";
            let important = important(&request);
            if preferences.system == Policy::Off
                || (preferences.system == Policy::Important && !important && !test) {
                self.suppressed += 1;
                continue;
            }
            self.recent.retain(|(_, at)| now.saturating_sub(*at) < 30_000);
            // Per-category cooldown plus global ceiling. No reconnect storms.
            let key = format!("{id}.{:?}", request.severity);
            if self.recent.len() >= 4 || self.recent.iter().any(|(seen, at)| seen == &key && (!test || now.saturating_sub(*at) < 2000)) {
                self.suppressed += 1;
                continue;
            }
            if self.recent.len() == 8 { self.recent.pop_front(); }
            self.recent.push_back((key, now));
            return Some(request);
        }
        None
    }
    pub fn result(&mut self, result: Result<(), String>) {
        match result {
            Ok(()) => {
                self.accepted += 1;
                self.last_result = "Windows Shell accepted the request; visibility is reported separately.".into();
            }
            Err(error) => { self.failed += 1; self.last_result = error; }
        }
    }
}
fn important(request: &ToastRequest) -> bool {
    matches!(request.severity, Severity::Warning | Severity::Error)
        || matches!(request.id.as_deref(), Some("pakar.settings" | "pakar.maintenance" | "pakar.connection" | "pakar.notification-test"))
}
/// OS notifications never copy raw API responses, URLs, names or credentials.
pub fn private_text(request: &ToastRequest) -> (&'static str, &'static str) {
    match request.id.as_deref() {
        Some("pakar.notification-test") => ("PakarController notification test", "Native Windows notifications are connected. Windows controls visibility and duration."),
        Some("pakar.connection") => ("PakarController connection changed", "The selected device connection changed. Open PakarController for its current status."),
        Some("pakar.maintenance") if request.severity == Severity::Success => ("PakarController maintenance completed", "The requested maintenance operation completed. Open PakarController to inspect the result."),
        Some("pakar.settings") if request.severity == Severity::Success => ("PakarController settings saved", "Saved public device settings were confirmed by readback. Secret values remain write-only."),
        _ if request.severity == Severity::Error => ("PakarController operation failed", "An operation did not complete. Open PakarController to inspect the error before retrying."),
        _ if request.severity == Severity::Warning => ("PakarController needs attention", "Open PakarController to inspect the warning."),
        _ => ("PakarController", "There is a new application message. Open PakarController for details."),
    }
}

/// None before the first authoritative freshness observation; initial offline
/// startup is not a disconnect event. A stale peer is not reported as online.
#[derive(Default)]
pub struct ConnectionNotice { previous: Option<Option<bool>> }
impl ConnectionNotice {
    pub fn reset(&mut self) { self.previous = None; }
    pub fn observe(&mut self, state: Option<bool>) -> Option<(&'static str, Severity)> {
        let previous = self.previous.replace(state)?;
        if previous == state { return None; }
        match state {
            None => Some(("Fresh status from the selected device was lost.", Severity::Warning)),
            Some(false) => Some(("The ESP is reachable, but it reports the AVR offline.", Severity::Warning)),
            Some(true) => Some(("Fresh device status is available and the ESP reports the AVR online.", Severity::Success)),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn request(id: &str, severity: Severity) -> ToastRequest {
        ToastRequest { id: Some(id.into()), title: "secret-title".into(), message: "https://user:password@private-host/?token=secret".into(), severity, timeout_ms: 5000 }
    }
    #[test]
    fn policy_keeps_routine_commands_out_of_windows_by_default() {
        let mut d = Delivery::default();
        d.enqueue(request("pakar.command", Severity::Success));
        d.enqueue(request("pakar.settings", Severity::Success));
        assert_eq!(d.next(&Preferences::default(), 0).unwrap().id.as_deref(), Some("pakar.settings"));
        assert_eq!(d.suppressed, 1);
        assert!(d.next(&Preferences::default(), 0).is_none());
    }
    #[test]
    fn bounded_queue_cooldown_off_and_test_never_replay() {
        let mut d = Delivery::default();
        for _ in 0..40 { d.enqueue(request("warning", Severity::Warning)); }
        assert!(d.next(&Preferences::default(), 10).is_some());
        assert!(d.next(&Preferences::default(), 10).is_none());
        assert_eq!(d.suppressed, 39);
        d.enqueue(request("warning", Severity::Warning));
        assert!(d.next(&Preferences::default(), 30_010).is_some());
        d.enqueue(request("pakar.notification-test", Severity::Info));
        assert!(d.next(&Preferences {system: Policy::Off, ..Default::default()}, 30_010).is_none());
        d.enqueue(request("pakar.notification-test", Severity::Info));
        assert!(d.next(&Preferences::default(), 30_010).is_some());
        assert!(d.next(&Preferences::default(), 30_010).is_none());
    }
    #[test]
    fn global_ceiling_and_no_raw_peer_text_in_os_notifications() {
        let mut d = Delivery::default();
        let p = Preferences {system: Policy::All, ..Default::default()};
        for i in 0..5 { d.enqueue(request(&format!("item{i}"), Severity::Error)); }
        for _ in 0..4 { assert!(d.next(&p, 10).is_some()); }
        assert!(d.next(&p, 10).is_none());
        for severity in [Severity::Info, Severity::Success, Severity::Warning, Severity::Error] {
            let (title, text) = private_text(&request("item", severity));
            assert!(!format!("{title}{text}").contains("secret"));
            assert!(!text.contains("private-host"));
        }
    }
    #[test]
    fn success_does_not_suppress_the_following_failure() {
        let mut d = Delivery::default();
        d.enqueue(request("pakar.maintenance", Severity::Success));
        d.enqueue(request("pakar.maintenance", Severity::Error));
        assert_eq!(d.next(&Preferences::default(), 10).unwrap().severity, Severity::Success);
        assert_eq!(d.next(&Preferences::default(), 10).unwrap().severity, Severity::Error);
    }
    #[test]
    fn routine_burst_cannot_evict_important_pending_alerts() {
        let mut d = Delivery::default();
        d.enqueue(request("critical", Severity::Error));
        for _ in 0..40 { d.enqueue(request("routine", Severity::Info)); }
        assert_eq!(d.next(&Preferences::default(), 1).unwrap().id.as_deref(), Some("critical"));
        assert!(d.next(&Preferences::default(), 1).is_none());
        let mut all_critical = Delivery::default();
        for i in 0..32 { all_critical.enqueue(request(&format!("critical{i}"), Severity::Error)); }
        all_critical.enqueue(request("routine", Severity::Info));
        assert_eq!(all_critical.queue.len(), 32);
        assert_eq!(all_critical.queue.front().unwrap().id.as_deref(), Some("critical0"));
    }
    #[test]
    fn omitted_saved_notification_preferences_have_explicit_safe_defaults() {
        let p: crate::config::AppConfig = serde_json::from_str("{}").unwrap();
        assert_eq!(p.notifications, Preferences::default());
        let changed = Preferences {in_app: false, system: Policy::Off, sound: true};
        assert_eq!(serde_json::from_str::<Preferences>(&serde_json::to_string(&changed).unwrap()).unwrap(), changed);
    }
    #[test]
    fn connection_alerts_follow_fresh_authority_not_repeated_poll_failures() {
        let mut c = ConnectionNotice::default();
        assert!(c.observe(None).is_none());
        assert!(c.observe(None).is_none());
        assert_eq!(c.observe(Some(true)).unwrap().1, Severity::Success);
        assert!(c.observe(Some(true)).is_none());
        assert_eq!(c.observe(Some(false)).unwrap().1, Severity::Warning);
        assert_eq!(c.observe(None).unwrap().1, Severity::Warning);
        assert!(c.observe(None).is_none());
        c.reset();
        assert!(c.observe(Some(true)).is_none());
    }
}
