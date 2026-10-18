//! Deliberate one-frame diagnostics, not an animation engine or a replay queue.
use crate::{DeviceStatus, Endpoint, peer_http::Pin};
use reqwest::blocking::Client;
use std::time::{Duration, Instant};

pub struct Request {
    pub id: u64,
    pub index: u16,
    pub peer: Pin,
    pub endpoint: Endpoint,
    pub at: Instant,
}
#[derive(Default)]
pub struct State {
    pub index: u16,
    pub pending: Option<(u64, Instant)>,
    pub armed: Option<Pin>,
    pub message: String,
    next_id: u64,
}
impl State {
    pub fn begin(&mut self) -> u64 {
        self.next_id = self.next_id.wrapping_add(1).max(1);
        self.pending = Some((self.next_id, Instant::now()));
        self.next_id
    }
    pub fn matches(&self, id: u64) -> bool { self.pending.is_some_and(|(active, _)| active == id) }
    pub fn finish(&mut self, id: u64, message: String) {
        if self.matches(id) { self.pending = None; self.message = message; }
    }
    pub fn revoke(&mut self) { self.pending = None; self.armed = None; self.message.clear(); }
    pub fn expire(&mut self) {
        if self.pending.is_some_and(|(_, at)| at.elapsed() >= Duration::from_secs(12)) {
            self.pending = None;
            self.message = "Diagnostic outcome unavailable; nothing was replayed. Next is a new deliberate frame.".into();
        }
    }
}
pub fn supported(s: &DeviceStatus) -> bool {
    s.pixels == 480 && Pin::from_status(s).is_some()
        && s.capabilities.as_ref().is_some_and(|m| m.has("leds") && m.has("streaming"))
}
pub fn ready(s: &DeviceStatus) -> bool {
    can_arm(s) && crate::led_output_mode(s) == Some("effects")
}
pub fn can_arm(s: &DeviceStatus) -> bool {
    supported(s) && !s.programming_latch && s.mode == "cell" && s.effect == "none"
        && s.led_output.get("enabled").and_then(|v| v.as_bool()) == Some(true)
}
/// Only the exact led-output state response can acknowledge an arm request.
pub fn mode_ack(text: &str) -> Option<Result<(), String>> {
    let value: serde_json::Value = serde_json::from_str(text).ok()?;
    let accepted = value.get("accepted")?.as_bool()?;
    let message = value.get("message")?.as_str()?;
    if !accepted { return Some(Err("Device rejected diagnostic effects mode; no frame sent".into())); }
    let state: serde_json::Value = serde_json::from_str(message).ok()?;
    if state.get("mode").and_then(|v| v.as_str()) != Some("effects")
        || state.get("enabled").and_then(|v| v.as_bool()) != Some(true) { return None; }
    Some(Ok(()))
}
pub fn pixels(index: u16) -> Option<Vec<u8>> {
    if index >= 480 { return None; }
    let mut rgb = vec![0; 480 * 3];
    rgb[index as usize * 3..index as usize * 3 + 3].fill(255);
    Some(rgb)
}
fn read_status(client: &Client, endpoint: &Endpoint, pixels: bool) -> Result<DeviceStatus, String> {
    client.get(format!("{}/api/status{}", endpoint.base.trim_end_matches('/'), if pixels { "?pixels=1" } else { "" }))
        .basic_auth(&endpoint.user, Some(&endpoint.password)).send().map_err(crate::diagnostics::describe_error)?
        .error_for_status().map_err(crate::diagnostics::describe_error)?.json().map_err(crate::diagnostics::describe_error)
}
pub fn verify_frame(client: &Client, endpoint: &Endpoint, peer: &Pin, index: u16) -> Result<String, String> {
    let status = read_status(client, endpoint, true)?;
    if Pin::from_status(&status).as_ref() != Some(peer) || !ready(&status) { return Err("Board or diagnostic mode changed before pixel readback".into()); }
    let frame = status.pixel_frame.as_ref().filter(|frame| frame.valid()).ok_or("Pixel snapshot unavailable")?;
    let expected = pixels(index).ok_or("Pixel index outside 0..479")?;
    if !frame.rgb.as_bytes().chunks_exact(2).zip(expected).all(|(hex, byte)|
        std::str::from_utf8(hex).ok().and_then(|v| u8::from_str_radix(v, 16).ok()) == Some(byte)) {
        return Err("Pixel snapshot differs from the sent frame; not replayed".into());
    }
    Ok(format!("Pixel {} sent; exact RGB snapshot matched (not optical proof).", index + 1))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn each_manual_frame_has_only_one_white_pixel_and_all_others_black() {
        for index in [0, 1, 5, 6, 239, 240, 478, 479] {
            let rgb = pixels(index).unwrap();
            assert_eq!(rgb.len(), 1440);
            for (at, pixel) in rgb.chunks_exact(3).enumerate() {
                assert_eq!(pixel, if at == index as usize { &[255, 255, 255][..] } else { &[0, 0, 0][..] });
            }
        }
        assert!(pixels(480).is_none());
    }
    #[test]
    fn late_results_do_not_finish_a_new_manual_request_and_no_auto_advance() {
        let mut s = State { index: 17, ..Default::default() };
        let old = s.begin(); let current = s.begin();
        s.finish(old, "late".into()); assert!(s.matches(current));
        s.finish(current, "sent".into()); assert!(s.pending.is_none()); assert_eq!(s.index, 17);
        s.armed = None; s.revoke(); assert!(s.pending.is_none());
    }
    #[test]
    fn only_exact_output_ack_can_arm_not_pong_or_unrelated_acceptance() {
        for text in [r#"{"type":"pong"}"#, r#"{"accepted":true}"#, r#"{"accepted":true,"message":"{}"}"#,
            r#"{"accepted":true,"message":"{\"mode\":\"static\",\"enabled\":true}"}"#] {
            assert!(mode_ack(text).is_none());
        }
        assert!(mode_ack(r#"{"accepted":true,"message":"{\"mode\":\"effects\",\"enabled\":true}"}"#).unwrap().is_ok());
    }
}
