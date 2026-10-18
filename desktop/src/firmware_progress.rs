//! Adapts PCController hostui/update_progress.go: stage-local measured counters,
//! explicit unknown, terminal != transfer-complete, and peer-owned observations.
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::sync::{Arc, Mutex};

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
    if !matches!(j.phase.as_str(), "download_esp" | "download_avr") {
        p.fraction = None;
    }
    p.active = j.busy;
    p.verified = j.phase == "complete";
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
        assert!(p.verified && !p.active);
        s.boot_id = "other".into();
        assert!(observed(&s).is_none());
    }
}
