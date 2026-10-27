//! Runtime wire-driver observation. Shared rendering mode is deliberately separate.
use crate::DeviceStatus;

pub fn name(code: u8) -> Option<&'static str> {
    match code { 1 => Some("gpio-static"), 2 => Some("rmt-dma"), _ => None }
}
pub fn code(name: &str) -> Option<u8> {
    match name { "gpio-static" => Some(1), "rmt-dma" => Some(2), _ => None }
}
pub fn label(name: &str) -> &str {
    match name {
        "gpio-static" => "GPIO · simple",
        "rmt-dma" => "RMT + DMA",
        "none" => "No LED driver",
        _ => "Unknown",
    }
}
pub fn allowed(status: &DeviceStatus, backend: &str) -> bool {
    !status.programming_latch
        && status.capabilities.as_ref().is_some_and(|c| c.has("leds")
            && c.led_output().is_some_and(|driver| driver.supports_backend(backend)))
        && status.led_output["driver"]["switch_pending"].as_bool() == Some(false)
}
pub fn pending(status: &DeviceStatus, backend: &str) -> bool {
    let driver = &status.led_output["driver"];
    driver["switch_pending"] == true && driver["requested_backend"] == backend
}
pub fn confirmed(status: &DeviceStatus, backend: &str) -> bool {
    let driver = &status.led_output["driver"];
    code(backend).is_some()
        && driver["backend"] == backend && driver["requested_backend"] == backend
        && driver["switch_pending"] == false && driver["ready"] == true
        && driver["switch_error"].as_i64() == Some(0)
        && status.led_output["backend_saved"] == true
        && status.led_output["backend_settings_error"].as_str() == Some("")
}
/// A bounded observation-only follow-up after acceptance. Never resends the
/// mutation, never crosses a board/boot or retired transport owner.
pub fn finish_network(
    endpoint: &crate::Endpoint,
    mut status: DeviceStatus,
    backend: &str,
    admit: impl Fn() -> Result<(), String>,
) -> Result<DeviceStatus, String> {
    use std::time::{Duration, Instant};
    let peer = crate::peer_http::Pin::from_status(&status).ok_or("LED backend peer unavailable")?;
    let deadline = Instant::now() + Duration::from_secs(2);
    while pending(&status, backend) {
        admit()?;
        let remaining = deadline.saturating_duration_since(Instant::now());
        if remaining.is_zero() {
            return Err("LED backend switch still pending; outcome unverified, nothing replayed".into());
        }
        std::thread::sleep(Duration::from_millis(40).min(remaining));
        admit()?;
        status = peer.client()?.get(format!("{}/api/status", endpoint.base.trim_end_matches('/')))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .timeout(deadline.saturating_duration_since(Instant::now()).max(Duration::from_millis(1)))
            .send().and_then(|response| response.error_for_status())
            .and_then(|response| response.json())
            .map_err(|_| "LED backend readback unavailable; outcome unknown, nothing replayed")?;
        if crate::peer_http::Pin::from_status(&status).as_ref() != Some(&peer) {
            return Err("LED backend readback belongs to another board/boot; nothing replayed".into());
        }
        if status.compatibility.is_some() { crate::peer_identity::validate_board(&status)?; }
    }
    admit()?;
    if confirmed(&status, backend) { Ok(status) }
    else { Err(format!("LED backend not applied or superseded (active {}, switch error {}, settings error {}); nothing replayed",
        status.led_output["driver"]["backend"], status.led_output["driver"]["switch_error"],
        status.led_output["backend_settings_error"])) }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn accepted_request_is_not_applied_driver_or_persistence() {
        let mut status: DeviceStatus = serde_json::from_value(serde_json::json!({
            "led_output":{"backend_saved":true,"backend_settings_error":"","driver":{
                "backend":"gpio-static","requested_backend":"rmt-dma","switch_pending":true,
                "switch_error":0,"ready":true}}
        })).unwrap();
        assert!(pending(&status, "rmt-dma"));
        assert!(!confirmed(&status, "rmt-dma"));
        status.led_output["driver"]["backend"] = "rmt-dma".into();
        status.led_output["driver"]["switch_pending"] = false.into();
        assert!(confirmed(&status, "rmt-dma"));
        for (section, field, value) in [
            (true, "ready", serde_json::json!(false)),
            (true, "switch_error", serde_json::json!(257)),
            (false, "backend_saved", serde_json::json!(false)),
            (false, "backend_settings_error", serde_json::json!("backend_commit_failed")),
        ] {
            let mut rejected = status.clone();
            if section { rejected.led_output["driver"][field] = value; }
            else { rejected.led_output[field] = value; }
            assert!(!confirmed(&rejected, "rmt-dma"));
        }
        assert!(!confirmed(&DeviceStatus::default(), "gpio-static"));
    }
    #[test]
    fn network_completion_observes_once_without_replaying_write() {
        use std::io::{Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let endpoint = crate::Endpoint {
            base: format!("http://{}", listener.local_addr().unwrap()),
            user: String::new(), password: String::new(),
        };
        let mut status: DeviceStatus = serde_json::from_value(serde_json::json!({
            "board_id":"esp32s3-fixture","boot_id":"a".repeat(32),"http_peer_fencing":true,
            "led_output":{"backend_saved":true,"backend_settings_error":"","driver":{
                "backend":"gpio-static","requested_backend":"rmt-dma","switch_pending":true,
                "switch_error":0,"ready":true}}
        })).unwrap();
        let mut applied = status.clone();
        applied.led_output["driver"]["backend"] = "rmt-dma".into();
        applied.led_output["driver"]["switch_pending"] = false.into();
        let body = serde_json::to_string(&applied).unwrap();
        let server = std::thread::spawn(move || {
            let (mut socket, _) = listener.accept().unwrap();
            socket.set_read_timeout(Some(std::time::Duration::from_secs(2))).unwrap();
            let mut request = Vec::new();
            loop {
                let mut chunk = [0; 1024];
                let count = socket.read(&mut chunk).unwrap();
                assert!(count > 0);
                request.extend_from_slice(&chunk[..count]);
                if request.windows(4).any(|window| window == b"\r\n\r\n") { break; }
            }
            assert!(String::from_utf8_lossy(&request).starts_with("GET /api/status "));
            write!(socket, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}", body.len(), body).unwrap();
        });
        assert!(confirmed(&finish_network(&endpoint, status.clone(), "rmt-dma", || Ok(())).unwrap(), "rmt-dma"));
        server.join().unwrap();
        // An owner retirement is checked before any further network I/O.
        assert!(finish_network(&endpoint, status.clone(), "rmt-dma", || Err("retired".into())).unwrap_err().contains("retired"));
        status.led_output["driver"]["requested_backend"] = "gpio-static".into();
        assert!(finish_network(&endpoint, status, "rmt-dma", || Ok(())).unwrap_err().contains("superseded"));
    }
}
