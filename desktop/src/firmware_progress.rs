//! Adapts PCController hostui/update_progress.go: stage-local measured counters,
//! explicit unknown, terminal != transfer-complete, and peer-owned observations.
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

/// Same board/current installed boot owns the lifecycle. source_boot_id may
/// identify an earlier upload boot only for a verified post-reboot receipt.
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Upgrade {
    pub board_id: String,
    pub boot_id: String,
    pub source_boot_id: String,
    pub source: String,
    pub operation_id: u64,
    pub revision: u64,
    pub phase: String,
    pub active: bool,
    pub received_bytes: u64,
    pub expected_bytes: u64,
    pub progress_known: bool,
    pub progress_percent: Option<f64>,
    pub completion_verified: bool,
    pub error: String,
    #[serde(skip)]
    observed_at: Option<Instant>,
}
pub fn optional_upgrade<'de, D: serde::Deserializer<'de>>(
    deserializer: D,
) -> Result<Option<Upgrade>, D::Error> {
    // A malformed additive progress object must not disconnect a valid board.
    let raw = Value::deserialize(deserializer)?;
    Ok(serde_json::from_value::<Upgrade>(raw)
        .ok()
        .filter(Upgrade::valid))
}
impl Upgrade {
    fn valid(&self) -> bool {
        let boot = |value: &str| {
            value.len() == 32
                && value
                    .bytes()
                    .all(|v| v.is_ascii_digit() || (b'a'..=b'f').contains(&v))
                && value.bytes().any(|v| v != b'0')
        };
        !self.board_id.is_empty()
            && self.board_id.len() <= 64
            && self
                .board_id
                .bytes()
                .all(|v| v.is_ascii_lowercase() || v.is_ascii_digit() || v == b'-' || v == b'_')
            && boot(&self.boot_id)
            && boot(&self.source_boot_id)
            && self.source.len() <= 64
            && !self.source.is_empty()
            && !self.source.chars().any(char::is_control)
            && !self.phase.is_empty()
            && self.phase.len() <= 63
            && !self.phase.chars().any(char::is_control)
            && self.error.len() <= 1024
            && !self
                .error
                .chars()
                .any(|v| v.is_control() && v != '\n' && v != '\t')
            && (self.expected_bytes == 0 || self.received_bytes <= self.expected_bytes)
            && (!self.active || self.source_boot_id == self.boot_id)
            && self
                .progress_percent
                .is_none_or(|p| p.is_finite() && (0.0..=100.0).contains(&p))
    }
    pub fn belongs(&self, status: &crate::DeviceStatus) -> bool {
        self.valid()
            && crate::peer_http::Pin::from_status(status)
                .is_some_and(|peer| peer.matches_identity(&self.board_id, &self.boot_id))
    }
    pub fn fresh(&self) -> bool {
        self.observed_at
            .is_some_and(|at| at.elapsed() < Duration::from_secs(5))
    }
    fn measured_fraction(&self) -> Option<f32> {
        if !self.progress_known
            || self.expected_bytes == 0
            || self.received_bytes > self.expected_bytes
            || !matches!(
                self.phase.as_str(),
                "receiving"
                    | "download_esp"
                    | "download_avr"
                    | "backup_flash"
                    | "backup_eeprom"
                    | "verify_flash"
                    | "verify_eeprom"
                    | "verify_esp"
                    | "program_avr"
                    | "program_esp"
            )
        {
            return None;
        }
        let percent =
            (u128::from(self.received_bytes) * 100 / u128::from(self.expected_bytes)) as u8;
        if self
            .progress_percent
            .is_some_and(|claimed| claimed != f64::from(percent))
        {
            return None;
        }
        Some(self.received_bytes as f32 / self.expected_bytes as f32)
    }
    fn observation(&self, status: &crate::DeviceStatus) -> Option<Observation> {
        if !self.belongs(status)
            || self.operation_id == 0
            || self.phase == "idle"
            || self.source == "boot"
        {
            return None;
        }
        let mut value =
            Observation::transfer(&self.phase, self.received_bytes, Some(self.expected_bytes));
        value.source = format!("Board · {} #{}", self.source, self.operation_id);
        value.fraction = self.measured_fraction();
        value.active = self.active;
        value.verified = !self.active && self.phase == "complete" && self.completion_verified;
        value.error = self.error.clone();
        Some(value)
    }
}
pub fn websocket_upgrade(text: &str) -> Option<Upgrade> {
    let raw: Value = serde_json::from_str(text).ok()?;
    if raw["type"] != "update-progress" {
        return None;
    }
    serde_json::from_value::<Upgrade>(raw)
        .ok()
        .filter(Upgrade::valid)
}
pub fn observe_upgrade(status: &mut crate::DeviceStatus, mut incoming: Upgrade) -> bool {
    if !incoming.belongs(status)
        || status
            .upgrade
            .as_ref()
            .filter(|old| old.belongs(status))
            .is_some_and(|old| incoming.revision <= old.revision)
    {
        return false;
    }
    incoming.observed_at = Some(Instant::now());
    status.upgrade = Some(incoming);
    true
}
pub fn retain_upgrade(status: &mut crate::DeviceStatus, previous: Option<&crate::DeviceStatus>) {
    let incoming = status.upgrade.take();
    let old = previous
        .and_then(|previous| previous.upgrade.as_ref())
        .filter(|old| old.belongs(status))
        .cloned();
    status.upgrade = old;
    if let Some(incoming) = incoming {
        observe_upgrade(status, incoming);
    }
}
pub fn lifecycle_observed(status: &crate::DeviceStatus) -> Option<Observation> {
    status.upgrade.as_ref()?.observation(status)
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Observation {
    pub source: String,
    pub phase: String,
    pub bytes_done: u64,
    pub bytes_total: Option<u64>,
    pub fraction: Option<f32>,
    pub active: bool,
    pub verified: bool,
    pub error: String,
}
impl Observation {
    pub fn transfer(phase: &str, done: u64, total: Option<u64>) -> Self {
        let total = total.filter(|v| *v > 0 && done <= *v);
        Self {
            source: "Application".into(),
            phase: phase.into(),
            bytes_done: done,
            bytes_total: total,
            fraction: total.map(|n| done as f32 / n as f32),
            active: true,
            verified: false,
            error: String::new(),
        }
    }
    pub fn label(&self) -> String {
        if !self.error.is_empty() {
            return format!("{} · {} · {}", self.source, self.phase, self.error);
        }
        let measured = self
            .fraction
            .map(|v| format!(" · {:.0}%", v * 100.0))
            .unwrap_or_default();
        format!(
            "{} · {}{}",
            self.source,
            self.phase.replace('_', " "),
            measured
        )
    }
}
fn bytes(v: &Value, key: &str) -> Option<u64> {
    v.get(key)?.as_u64()
}
pub fn observed(status: &crate::DeviceStatus) -> Option<Observation> {
    if let Some(progress) = lifecycle_observed(status) {
        return Some(progress);
    }
    let a = &status.avr_maintenance;
    if !status.boot_id.is_empty()
        && a["boot_id"].as_str() == Some(status.boot_id.as_str())
        && matches!(a["method"].as_str(), Some("urboot" | "isp"))
        && (a["busy"] == true
            || (status.remote_firmware["busy"] != true
                && !matches!(
                    status.update["phase"].as_str(),
                    Some("receiving" | "verifying" | "ready-to-reboot")
                )))
        && a["job_id"].as_u64().is_some_and(|id| id > 0)
        && a["busy"].is_boolean()
        && a["phase"].as_str().is_some_and(|p| {
            matches!(
                p,
                "queued"
                    | "prepare"
                    | "backup_isp"
                    | "program_isp"
                    | "backup_flash"
                    | "backup_eeprom"
                    | "program_avr"
                    | "verify_flash"
                    | "verify_eeprom"
                    | "awaiting_application"
                    | "complete"
                    | "failed"
            )
        })
    {
        let phase = a["phase"].as_str()?;
        let done = bytes(a, "received_bytes")?;
        let total = bytes(a, "expected_bytes")?;
        if done > total {
            return None;
        }
        let mut p = Observation::transfer(phase, done, Some(total));
        p.source = format!(
            "AVR · {} #{}",
            if a["method"] == "isp" {
                "ISP"
            } else {
                "Urboot"
            },
            a["job_id"]
        );
        p.active = a["busy"] == true;
        p.verified =
            phase == "complete" && a["verified"] == true && a["application_recovered"] == true;
        p.error = a["error"].as_str().unwrap_or_default().to_string();
        if !matches!(
            phase,
            "backup_flash" | "backup_eeprom" | "program_avr" | "verify_flash" | "verify_eeprom"
        ) {
            p.fraction = None;
        }
        return Some(p);
    }
    let local = &status.update;
    if let Some(phase) = local["phase"].as_str().filter(|p| {
        matches!(
            *p,
            "receiving" | "verifying" | "ready-to-reboot" | "failed" | "aborted"
        )
    }) {
        let done = bytes(local, "received_bytes")?;
        let total = bytes(local, "expected_bytes").filter(|n| *n > 0 && done <= *n);
        let mut p = Observation::transfer(phase, done, total);
        p.source = "Board OTA".into();
        p.fraction = if phase == "receiving" && local["progress_known"] == true {
            p.fraction
        } else {
            None
        };
        p.active = !matches!(phase, "failed" | "aborted");
        p.verified = local["completion_verified"] == true;
        return Some(p);
    }
    let j = crate::maintenance::parse_job(status.remote_firmware.clone()).ok()?;
    if j.job_id == 0 || j.phase == "idle" || j.boot_id != status.boot_id {
        return None;
    }
    let mut p = Observation::transfer(
        &j.phase,
        u64::from(j.received_bytes),
        Some(u64::from(j.expected_bytes)),
    );
    p.source = format!("Board update #{}", j.job_id);
    if !matches!(j.phase.as_str(), "download_esp" | "download_avr")
        || status.remote_firmware["progress_known"] != true
    {
        p.fraction = None;
    }
    p.active = j.busy;
    p.verified =
        j.phase == "complete" && !j.busy && status.remote_firmware["completion_verified"] == true;
    p.error = j.error;
    Some(p)
}

#[derive(Clone)]
pub struct Transfer {
    pub scope: crate::peer_http::Scope,
    pub dispatch_deadline: Option<u64>,
    board: String,
    boot: String,
    value: Arc<Mutex<Observation>>,
}
impl Transfer {
    pub fn new(scope: crate::peer_http::Scope, status: &crate::DeviceStatus) -> Self {
        Self {
            scope,
            dispatch_deadline: None,
            board: status.board_id.clone(),
            boot: status.boot_id.clone(),
            value: Arc::new(Mutex::new(Observation::transfer("queued", 0, None))),
        }
    }
    pub fn admit(&self) -> Result<(), String> {
        if self.scope.current() {
            Ok(())
        } else {
            Err("Firmware owner changed; no request replayed".into())
        }
    }
    pub fn before_upload(&self) -> Result<(), String> {
        self.admit()?;
        if self
            .dispatch_deadline
            .is_some_and(|at| crate::firmware_feed::now_ms() >= at)
        {
            return Err("Firmware offer expired before upload; no board write".into());
        }
        Ok(())
    }
    pub fn set(&self, phase: &str, done: u64, total: Option<u64>) {
        if let Ok(mut value) = self.value.lock() {
            *value = Observation::transfer(phase, done, total);
        }
    }
    pub fn finish(&self, result: &Result<String, String>) {
        if let Ok(mut v) = self.value.lock() {
            v.active = false;
            v.fraction = None;
            match result {
                Ok(_) => v.phase = "accepted · inspect board verification/reboot".into(),
                Err(e) => {
                    v.phase = "outcome needs inspection".into();
                    v.error = e.clone();
                }
            }
        }
    }
    pub fn snapshot(&self, status: Option<&crate::DeviceStatus>) -> Option<Observation> {
        let s = status?;
        if s.board_id != self.board || s.boot_id != self.boot {
            return None;
        }
        self.value.lock().ok().map(|v| v.clone())
    }
}
pub fn draw(ui: &mut eframe::egui::Ui, p: &Observation) {
    ui.label(p.label());
    if p.active {
        ui.add(
            eframe::egui::ProgressBar::new(p.fraction.unwrap_or(0.0))
                .animate(p.fraction.is_none())
                .desired_width(ui.available_width().min(480.0)),
        );
    }
    if let Some(total) = p.bytes_total {
        ui.weak(format!("{} / {} bytes · this stage", p.bytes_done, total));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn upgrade_status() -> crate::DeviceStatus {
        serde_json::from_value(serde_json::json!({"hostname":"ledboard","board_id":"esp32s3-fixture","boot_id":"a".repeat(32),"http_peer_fencing":true,
            "cells":["white"],"upgrade":{"board_id":"esp32s3-fixture","boot_id":"a".repeat(32),"source_boot_id":"a".repeat(32),
                "source":"local-ota","operation_id":7,"revision":3,"phase":"receiving","active":true,
                "received_bytes":50,"expected_bytes":100,"progress_known":true,"progress_percent":50,
                "completion_verified":false,"error":""}})).unwrap()
    }
    #[test]
    fn lifecycle_progress_prefers_current_peer_and_never_invents_known_percentage_or_completion() {
        let mut s = upgrade_status();
        s.update = serde_json::json!({"phase":"failed","received_bytes":0,"expected_bytes":0});
        let p = observed(&s).unwrap();
        assert!(p.source.contains("local-ota #7"));
        assert_eq!(p.fraction, Some(0.5));
        let upgrade = s.upgrade.as_mut().unwrap();
        upgrade.progress_known = false;
        upgrade.progress_percent = Some(99.0);
        assert!(observed(&s).unwrap().fraction.is_none());
        let upgrade = s.upgrade.as_mut().unwrap();
        upgrade.expected_bytes = 0;
        upgrade.progress_known = true;
        assert!(observed(&s).unwrap().fraction.is_none());
        let upgrade = s.upgrade.as_mut().unwrap();
        upgrade.phase = "complete".into();
        upgrade.active = false;
        upgrade.source_boot_id = "b".repeat(32);
        assert!(
            !observed(&s).unwrap().verified,
            "A historical complete phase is not completion proof"
        );
        s.upgrade.as_mut().unwrap().completion_verified = true;
        assert!(observed(&s).unwrap().verified);
        s.upgrade.as_mut().unwrap().phase = "ready-to-reboot".into();
        assert!(!observed(&s).unwrap().verified);
        s.upgrade.as_mut().unwrap().board_id = "other-board".into();
        assert!(lifecycle_observed(&s).is_none());
        s.upgrade = upgrade_status().upgrade;
        s.boot_id = "b".repeat(32);
        assert!(lifecycle_observed(&s).is_none());
    }
    #[test]
    fn lifecycle_fraction_uses_counters_not_rounded_percent_and_prepare_stays_indeterminate() {
        let mut status = upgrade_status();
        let upgrade = status.upgrade.as_mut().unwrap();
        upgrade.received_bytes = 1;
        upgrade.expected_bytes = 3;
        upgrade.progress_percent = Some(33.0);
        assert!((observed(&status).unwrap().fraction.unwrap() - 1.0 / 3.0).abs() < 0.00001);
        status.upgrade.as_mut().unwrap().progress_percent = Some(34.0);
        assert!(
            observed(&status).unwrap().fraction.is_none(),
            "Incoherent rounded percentage cannot become measured progress"
        );
        status.upgrade.as_mut().unwrap().progress_percent = None;
        assert!((observed(&status).unwrap().fraction.unwrap() - 1.0 / 3.0).abs() < 0.00001);
        status.upgrade.as_mut().unwrap().phase = "prepare".into();
        assert!(observed(&status).unwrap().fraction.is_none());
    }
    #[test]
    fn incremental_upgrade_events_do_not_rewind_or_promote_status_authority() {
        let mut s = upgrade_status();
        let original = s.clone();
        let mut event = s.upgrade.clone().unwrap();
        event.revision = 4;
        event.received_bytes = 75;
        event.progress_percent = Some(75.0);
        let mut wire = serde_json::to_value(&event).unwrap();
        wire["type"] = "update-progress".into();
        let event = websocket_upgrade(&wire.to_string()).unwrap();
        assert!(observe_upgrade(&mut s, event.clone()));
        assert!(s.upgrade.as_ref().unwrap().fresh());
        assert_eq!(observed(&s).unwrap().fraction, Some(0.75));
        assert_eq!(s.hostname, original.hostname);
        assert_eq!(s.cells, original.cells);
        assert!(!observe_upgrade(&mut s, event));
        let mut delayed = original.clone();
        retain_upgrade(&mut delayed, Some(&s));
        assert_eq!(delayed.upgrade.as_ref().unwrap().revision, 4);
        let mut foreign = delayed.upgrade.clone().unwrap();
        foreign.boot_id = "b".repeat(32);
        foreign.revision = 99;
        assert!(!observe_upgrade(&mut s, foreign));
        assert_eq!(s.upgrade.as_ref().unwrap().revision, 4);
        delayed.boot_id = "b".repeat(32);
        retain_upgrade(&mut delayed, Some(&s));
        assert!(delayed.upgrade.is_none());
        wire["type"] = "command-result".into();
        assert!(websocket_upgrade(&wire.to_string()).is_none());
    }
    #[test]
    fn optional_invalid_upgrade_keeps_legacy_status_readable() {
        for value in [
            Value::Null,
            serde_json::json!({"phase":"complete"}),
            serde_json::json!(17),
        ] {
            let s: crate::DeviceStatus =
                serde_json::from_value(serde_json::json!({"hostname":"ledboard","upgrade":value}))
                    .unwrap();
            assert_eq!(s.hostname, "ledboard");
            assert!(s.upgrade.is_none());
        }
        let mut s = upgrade_status();
        let u = s.upgrade.as_mut().unwrap();
        u.source_boot_id = "b".repeat(32);
        let encoded = serde_json::to_value(&s).unwrap();
        let parsed: crate::DeviceStatus = serde_json::from_value(encoded).unwrap();
        assert!(parsed.upgrade.is_none());
    }
    #[test]
    fn firmware_progress_urboot_requires_same_boot_and_application_recovery() {
        let mut s = crate::DeviceStatus {
            boot_id: "boot".into(),
            ..Default::default()
        };
        s.avr_maintenance = serde_json::json!({"job_id":1,"boot_id":"boot","method":"urboot","phase":"program_avr","busy":true,"received_bytes":128,"expected_bytes":256});
        assert_eq!(observed(&s).unwrap().fraction, Some(0.5));
        s.avr_maintenance["phase"] = "awaiting_application".into();
        assert!(observed(&s).unwrap().fraction.is_none());
        s.avr_maintenance["phase"] = "complete".into();
        s.avr_maintenance["verified"] = true.into();
        assert!(!observed(&s).unwrap().verified);
        s.avr_maintenance["application_recovered"] = true.into();
        assert!(observed(&s).unwrap().verified);
        s.avr_maintenance["method"] = "isp".into();
        s.avr_maintenance["phase"] = "program_isp".into();
        assert!(observed(&s).unwrap().fraction.is_none());
        assert!(observed(&s).unwrap().source.contains("ISP"));
        s.boot_id = "other".into();
        assert!(observed(&s).is_none());
    }
    #[test]
    fn firmware_progress_rejects_expired_offer_and_retired_owner_before_upload() {
        let (tx, _) = std::sync::mpsc::channel();
        let mut queue = crate::peer_http::Queue::new(tx);
        let status = crate::DeviceStatus {
            board_id: "board".into(),
            boot_id: "a".repeat(32),
            http_peer_fencing: true,
            ..Default::default()
        };
        queue.observe(&status);
        let mut progress = Transfer::new(queue.capture(), &status);
        progress.dispatch_deadline = Some(0);
        assert!(progress.before_upload().unwrap_err().contains("expired"));
        progress.dispatch_deadline = None;
        assert!(progress.before_upload().is_ok());
        queue.clear();
        assert!(progress.before_upload().is_err());
    }
    #[test]
    fn firmware_progress_observes_external_ota_without_inventing_completion() {
        let mut s = crate::DeviceStatus::default();
        s.update = serde_json::json!({"phase":"receiving","received_bytes":50,"expected_bytes":100,"progress_known":true,"completion_verified":false});
        let p = observed(&s).unwrap();
        assert_eq!(p.fraction, Some(0.5));
        assert!(p.active && !p.verified);
        s.update["phase"] = "verifying".into();
        s.update["received_bytes"] = 100.into();
        let p = observed(&s).unwrap();
        assert!(p.fraction.is_none() && p.active && !p.verified);
        s.update["phase"] = "ready-to-reboot".into();
        assert!(!observed(&s).unwrap().verified);
        s.update["phase"] = "aborted".into();
        assert!(!observed(&s).unwrap().active);
        s.update["received_bytes"] = "50".into();
        assert!(observed(&s).is_none());
    }
    #[test]
    fn firmware_progress_remote_counts_are_not_flash_percentage_or_another_boot() {
        let mut s = crate::DeviceStatus {
            boot_id: "boot".into(),
            ..Default::default()
        };
        s.remote_firmware = serde_json::json!({"job_id":7,"boot_id":"boot","phase":"program_esp","targets":1,
            "manifest_sha256":"a".repeat(64),"busy":true,"cancellable":false,"received_bytes":100,"expected_bytes":100,"error":""});
        let p = observed(&s).unwrap();
        assert!(p.active && p.fraction.is_none() && !p.verified);
        s.remote_firmware["phase"] = "complete".into();
        s.remote_firmware["busy"] = false.into();
        let p = observed(&s).unwrap();
        assert!(!p.verified && !p.active);
        s.remote_firmware["completion_verified"] = true.into();
        assert!(observed(&s).unwrap().verified);
        s.boot_id = "other".into();
        assert!(observed(&s).is_none());
    }
}
