use serde::{Deserialize, Serialize};
pub const PATH: &str = "/api/pixels/corrections";
pub const PREVIEW_PATH: &str = "/api/pixels/corrections/preview";
pub const PREVIEW_FEATURE: &str = "pixel-corrections-preview";
pub const PREVIEW_COMMAND_FEATURE: &str = "pixel-corrections-preview-command";
pub const PREVIEW_TTL_MS: u64 = 10_000;
pub fn rejection(reason: &str) -> String {
    let detail = match reason {
        "corrections_preview_owned" => {
            "Another editor owns this preview. Wait for its lease to expire, then edit again"
        }
        "corrections_preview_expired" => {
            "The preview expired. Toggle preview off/on and make a fresh edit"
        }
        "corrections_conflict" => "Saved corrections changed. Reload them before editing",
        _ => "Preview rejected; nothing was saved or replayed",
    };
    format!("{detail} ({reason})")
}
#[derive(Clone, Copy)]
pub struct DiagnosticCursor {
    pub raw: u16,
    pub revision: u32,
}
fn unique_wire(map: &[i16], raw: u16) -> Result<u16, String> {
    let matches: Vec<_> = map
        .iter()
        .enumerate()
        .filter(|(_, source)| **source == raw as i16)
        .map(|(wire, _)| wire)
        .collect();
    match matches.as_slice() {
        [wire] => Ok(*wire as u16),
        [] => {
            Err("Raw diagnostic pixel is absent here; choose an output address explicitly".into())
        }
        _ => Err(
            "Raw diagnostic pixel reaches multiple outputs; choose an output address explicitly"
                .into(),
        ),
    }
}
#[derive(Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Rule {
    pub kind: String,
    pub pixel: u16,
    pub offset: i16,
    #[serde(default = "enabled_by_default")]
    pub enabled: bool,
}
const fn enabled_by_default() -> bool {
    true
}
impl Rule {
    /// The stored address is always zero-based. Only presentation is one-based;
    /// never reinterpret a previously saved rule when loading another client.
    pub fn human_pixel(&self) -> u16 {
        self.pixel.saturating_add(1)
    }
    pub fn set_human_pixel(&mut self, pixel: u16, pixels: u16) -> bool {
        if !(1..=pixels).contains(&pixel) {
            return false;
        }
        let changed = self.pixel != pixel - 1;
        self.pixel = pixel - 1;
        changed
    }
    /// Integer adaptation of Pealayer's bounded numeric adjustment: never
    /// alter another rule, its enabled flag or the last board-owned mapping.
    pub fn step_pixel(&mut self, delta: i16, pixels: u16) -> bool {
        if pixels == 0 {
            return false;
        }
        let next =
            (i32::from(self.pixel) + i32::from(delta)).clamp(0, i32::from(pixels) - 1) as u16;
        let changed = next != self.pixel;
        self.pixel = next;
        changed
    }
    pub fn step_offset(&mut self, delta: i16, pixels: u16) -> bool {
        if pixels == 0 || self.kind != "shift" {
            return false;
        }
        let bound = i32::from(pixels.min(i16::MAX as u16));
        let next = (i32::from(self.offset) + i32::from(delta)).clamp(-bound, bound) as i16;
        let changed = next != self.offset;
        self.offset = next;
        changed
    }
}
#[derive(Clone, Default, Serialize, Deserialize)]
pub struct Configuration {
    pub revision: u32,
    pub enabled: bool,
    pub rules: Vec<Rule>,
    #[serde(default)]
    pub pixels: u16,
    #[serde(default)]
    pub sources: Vec<i16>,
    #[serde(default)]
    pub active_sources: Vec<i16>,
    #[serde(default)]
    pub preview: PreviewLease,
}
#[derive(Clone, Default, Serialize, Deserialize)]
pub struct PreviewLease {
    pub active: bool,
    pub lease_id: u32,
    pub base_revision: u32,
    pub remaining_ms: u32,
}
impl Configuration {
    pub fn validate(&self) -> Result<(), String> {
        if self.pixels == 0 || self.pixels > 480 || self.rules.len() > 32 {
            return Err("Invalid correction capacity".into());
        }
        for rule in &self.rules {
            if rule.pixel >= self.pixels
                || !matches!(rule.kind.as_str(), "black" | "remove" | "shift")
                || i32::from(rule.offset).abs() > i32::from(self.pixels)
                || (rule.kind != "shift" && rule.offset != 0)
            {
                return Err("Invalid correction rule; saved addresses were not changed".into());
            }
        }
        Ok(())
    }
    pub fn write_value(&self) -> serde_json::Value {
        serde_json::json!({"revision":self.revision,"enabled":self.enabled,"rules":self.rules})
    }
    /// Diagnostics address raw scene sources. A rule addresses an intermediate
    /// outgoing slot after preceding rules, not that raw source's numeric ID.
    pub fn wire_for_cursor(
        &self,
        cursor: DiagnosticCursor,
        before_rule: usize,
    ) -> Result<u16, String> {
        if cursor.revision != self.revision
            || cursor.raw >= self.pixels
            || before_rule > self.rules.len()
        {
            return Err("Diagnostic cursor revision changed; read the board again".into());
        }
        self.validate()?;
        let mut prefix = self.clone();
        prefix.rules.truncate(before_rule);
        let map = prefix.expected_sources()?;
        unique_wire(&map, cursor.raw)
    }
    /// Exact integer mapping from BoardCore::PixelCorrections::configure. This
    /// verifies board readback, not a replacement effects engine or physical proof.
    pub fn expected_sources(&self) -> Result<Vec<i16>, String> {
        self.validate()?;
        let mut map: Vec<i16> = (0..self.pixels as i16).collect();
        if !self.enabled {
            return Ok(map);
        }
        let size = i32::from(self.pixels);
        for rule in self.rules.iter().filter(|rule| rule.enabled) {
            let pixel = i32::from(rule.pixel);
            let offset = i32::from(rule.offset);
            match rule.kind.as_str() {
                "black" => map[pixel as usize] = -1,
                "remove" => {
                    for i in pixel..size - 1 {
                        map[i as usize] = map[i as usize + 1];
                    }
                    map[size as usize - 1] = -1;
                }
                "shift" if offset > 0 => {
                    for i in (pixel..size).rev() {
                        map[i as usize] = if i - offset >= pixel {
                            map[(i - offset) as usize]
                        } else {
                            -1
                        };
                    }
                }
                "shift" if offset < 0 => {
                    let end = size + offset;
                    for i in (pixel + offset).max(0)..end {
                        map[i as usize] = map[(i - offset) as usize];
                    }
                    for i in end.max(pixel)..size {
                        map[i as usize] = -1;
                    }
                }
                _ => {}
            }
        }
        Ok(map)
    }
}
#[derive(Default)]
pub struct Editor {
    pub tab: bool,
    pub owner: Option<crate::peer_http::Pin>,
    pub value: Option<Configuration>,
    pub dirty: bool,
    pub message: String,
    pub pending: Option<std::time::Instant>,
    pub expected: Option<Configuration>,
    pub live: LivePreview,
    pub reload_after_preview: bool,
}

#[derive(Clone)]
pub struct PreviewRequest {
    pub id: u64,
    pub value: Configuration,
    pub lease: u32,
    pub clear: bool,
    pub at: std::time::Instant,
}
impl PreviewRequest {
    pub fn from_form(
        status: &crate::DeviceStatus,
        form: &[(String, String)],
    ) -> Result<Self, String> {
        if form.len() != 1 || form[0].0 != "configuration" {
            return Err("Invalid correction preview payload".into());
        }
        let payload: serde_json::Value =
            serde_json::from_str(&form[0].1).map_err(|_| "Invalid correction preview JSON")?;
        let revision = payload["revision"]
            .as_u64()
            .and_then(|v| u32::try_from(v).ok())
            .ok_or("Invalid correction revision")?;
        let lease = match payload.get("lease_id") {
            None => 0,
            Some(v) => v
                .as_u64()
                .and_then(|v| u32::try_from(v).ok())
                .ok_or("Invalid preview lease")?,
        };
        let clear = match payload.get("clear") {
            None => false,
            Some(v) => v.as_bool().ok_or("Invalid preview clear")?,
        };
        let pixels = u16::try_from(status.pixels).map_err(|_| "Invalid preview capacity")?;
        let mut value = if clear {
            Configuration {
                revision,
                pixels,
                ..Default::default()
            }
        } else {
            serde_json::from_value::<Configuration>(payload.clone())
                .map_err(|_| "Invalid preview configuration")?
        };
        value.pixels = pixels;
        value.validate()?;
        if clear && lease == 0 {
            return Err("Preview release needs an owned lease".into());
        }
        if !clear && payload["ttl_ms"].as_u64() != Some(PREVIEW_TTL_MS) {
            return Err("Invalid correction preview lease duration".into());
        }
        Ok(Self {
            id: 0,
            value,
            lease,
            clear,
            at: std::time::Instant::now(),
        })
    }
    pub fn payload(&self) -> serde_json::Value {
        if self.clear {
            serde_json::json!({"revision":self.value.revision,"lease_id":self.lease,"clear":true})
        } else {
            let mut value = self.value.write_value();
            value["lease_id"] = self.lease.into();
            value["ttl_ms"] = PREVIEW_TTL_MS.into();
            value
        }
    }
    pub fn verify(&self, actual: &Configuration) -> Result<(), String> {
        actual.validate()?;
        if actual.sources != actual.expected_sources()?
            || actual.active_sources.len() != usize::from(actual.pixels)
        {
            return Err("Incomplete or inconsistent correction mapping readback".into());
        }
        if actual.revision != self.value.revision
            || actual.pixels != self.value.pixels
            || actual.preview.base_revision != self.value.revision
        {
            return Err("Correction revision changed; preview was not confirmed".into());
        }
        if self.clear {
            if actual.preview.active
                || actual.preview.lease_id != 0
                || actual.active_sources != actual.sources
            {
                return Err("Preview release was not confirmed".into());
            }
        } else if !actual.preview.active
            || actual.preview.lease_id == 0
            || (self.lease != 0 && actual.preview.lease_id != self.lease)
            || actual.preview.remaining_ms == 0
            || u64::from(actual.preview.remaining_ms) > PREVIEW_TTL_MS
            || actual.active_sources != self.value.expected_sources()?
        {
            return Err("Live correction readback differs; no preview was confirmed".into());
        }
        Ok(())
    }
}

fn decode_response(response: reqwest::blocking::Response) -> Result<Configuration, String> {
    let status = response.status();
    let value: serde_json::Value = response.json().map_err(|_| "Invalid correction response")?;
    if !status.is_success() {
        return Err(format!(
            "HTTP {status}: {}",
            rejection(
                value["reason"]
                    .as_str()
                    .or_else(|| value["error"].as_str())
                    .unwrap_or("unknown")
            )
        ));
    }
    serde_json::from_value(value).map_err(|_| "Invalid correction readback".into())
}
fn finish_http_preview(
    client: &reqwest::blocking::Client,
    endpoint: &crate::Endpoint,
    peer: &crate::peer_http::Pin,
    request: &PreviewRequest,
    actual: Configuration,
    admit: impl Fn() -> Result<(), String>,
) -> Result<Configuration, String> {
    admit()?;
    request.verify(&actual)?;
    // The correction document has no producer envelope on older firmware.
    // Fenced headers plus a same-boot trailing status retain the existing guard.
    let after = crate::blocking_status(client, endpoint)?;
    admit()?;
    if crate::peer_http::Pin::from_status(&after).as_ref() != Some(peer) {
        return Err("Correction preview peer changed; nothing replayed".into());
    }
    Ok(actual)
}
pub fn send_http_preview(
    client: &reqwest::blocking::Client,
    endpoint: &crate::Endpoint,
    peer: &crate::peer_http::Pin,
    request: &PreviewRequest,
    admit: impl Fn() -> Result<(), String>,
) -> Result<Configuration, String> {
    admit()?;
    request.value.validate()?;
    let actual = decode_response(
        client
            .post(format!(
                "{}{}",
                endpoint.base.trim_end_matches('/'),
                PREVIEW_PATH
            ))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .json(&request.payload())
            .send()
            .map_err(|_| "Correction preview outcome unknown; no command replayed")?,
    )?;
    finish_http_preview(client, endpoint, peer, request, actual, admit)
}
pub fn read_preview(
    endpoint: &crate::Endpoint,
    peer: &crate::peer_http::Pin,
    request: &PreviewRequest,
    admit: impl Fn() -> Result<(), String>,
) -> Result<Configuration, String> {
    admit()?;
    let client = peer.client()?;
    let actual = decode_response(
        client
            .get(format!("{}{}", endpoint.base.trim_end_matches('/'), PATH))
            .basic_auth(&endpoint.user, Some(&endpoint.password))
            .send()
            .map_err(|_| "Correction preview readback unavailable; no command replayed")?,
    )?;
    finish_http_preview(&client, endpoint, peer, request, actual, admit)
}

/// Default-on is a preference, not startup authority. Only an explicit local
/// edit engages a lease. One request is in flight; successful observations renew
/// it while the editor stays visible. Failure stops, never retries a mutation.
pub struct LivePreview {
    pub enabled: bool,
    pub engaged: bool,
    pub pending: Option<PreviewRequest>,
    pub observed: Option<Configuration>,
    pub error: String,
    pub painted_at: Option<std::time::Instant>,
    lease: u32,
    next_id: u64,
    due: Option<std::time::Instant>,
    expires: Option<std::time::Instant>,
    release: bool,
    blocked: bool,
}
impl Default for LivePreview {
    fn default() -> Self {
        Self {
            enabled: true,
            engaged: false,
            pending: None,
            observed: None,
            error: String::new(),
            painted_at: None,
            lease: 0,
            next_id: 0,
            due: None,
            expires: None,
            release: false,
            blocked: false,
        }
    }
}
impl LivePreview {
    pub fn edited(&mut self, now: std::time::Instant) {
        if self.enabled && !self.blocked {
            self.engaged = true;
            self.due = Some(now + std::time::Duration::from_millis(280));
        }
    }
    pub fn toggle(&mut self, enabled: bool) {
        self.enabled = enabled;
        self.blocked = false;
        self.error.clear();
        if !enabled {
            self.stop();
        }
        // Re-enabling alone does not apply an old draft.
    }
    pub fn stop(&mut self) {
        self.engaged = false;
        self.due = None;
        self.release = self.lease != 0 || self.pending.is_some();
    }
    pub fn committed(&mut self) {
        let enabled = self.enabled;
        let next_id = self.next_id;
        *self = Self::default();
        self.enabled = enabled;
        self.next_id = next_id;
    }
    pub fn save_lease(&self) -> Option<u32> {
        (self.lease != 0).then_some(self.lease)
    }
    pub fn next(
        &mut self,
        value: &Configuration,
        now: std::time::Instant,
    ) -> Option<PreviewRequest> {
        if let Some(pending) = &self.pending {
            if now.saturating_duration_since(pending.at).as_millis() >= 8000 {
                self.failed("Preview receipt timed out; no command was replayed".into());
            }
            return None;
        }
        if self.expires.is_some_and(|expires| now >= expires) {
            self.lease = 0;
            self.observed = None;
            self.expires = None;
            self.failed("Live preview expired; saved corrections remain unchanged".into());
        }
        let clear = self.release && self.lease != 0;
        if !clear
            && (!self.enabled
                || !self.engaged
                || self.blocked
                || self.due.is_none_or(|due| now < due))
        {
            return None;
        }
        // Cleanup is bound to the last confirmed lease, never to a malformed
        // draft that the user may currently be correcting.
        let value = if clear {
            self.observed.as_ref().unwrap_or(value)
        } else {
            value
        };
        if let Err(error) = value.validate() {
            self.failed(error);
            return None;
        }
        let Some(next_id) = self.next_id.checked_add(1) else {
            self.failed("Preview request IDs exhausted; reopen the connection".into());
            return None;
        };
        self.next_id = next_id;
        let request = PreviewRequest {
            id: self.next_id,
            value: value.clone(),
            lease: self.lease,
            clear,
            at: now,
        };
        self.pending = Some(request.clone());
        self.release = false;
        self.due = None;
        Some(request)
    }
    pub fn finish(
        &mut self,
        id: u64,
        result: Result<Configuration, String>,
        now: std::time::Instant,
    ) {
        let Some(request) = self
            .pending
            .as_ref()
            .filter(|request| request.id == id)
            .cloned()
        else {
            return;
        };
        self.pending = None;
        let actual = match result.and_then(|actual| {
            request.verify(&actual)?;
            Ok(actual)
        }) {
            Ok(actual) => actual,
            Err(error) => {
                if request.clear {
                    self.lease = 0;
                    self.expires = None;
                    self.observed = None;
                }
                self.failed(error);
                return;
            }
        };
        if request.clear {
            self.lease = 0;
            self.observed = None;
            self.expires = None;
            self.release = false;
            return;
        }
        // Count transit against the lease; never assume a delayed reply bought
        // another full TTL. A cancelled in-flight acquire is released once.
        let remaining = std::time::Duration::from_millis(u64::from(actual.preview.remaining_ms))
            .min(
                std::time::Duration::from_millis(PREVIEW_TTL_MS)
                    .saturating_sub(now.saturating_duration_since(request.at)),
            );
        self.lease = actual.preview.lease_id;
        self.expires = Some(now + remaining);
        self.observed = Some(actual);
        if remaining < std::time::Duration::from_millis(500) {
            self.failed("Preview confirmation arrived too late; nothing was saved".into());
            return;
        }
        if !self.engaged || !self.enabled || self.release {
            self.release = true;
            return;
        }
        let renew = now + remaining / 2;
        self.due = Some(self.due.map_or(renew, |edit| edit.min(renew)));
    }
    pub fn failed(&mut self, error: String) {
        if self.pending.as_ref().is_some_and(|request| request.clear) {
            self.lease = 0;
            self.expires = None;
            self.observed = None;
        }
        self.pending = None;
        self.engaged = false;
        self.due = None;
        self.blocked = true;
        self.error = error;
        self.release = self.lease != 0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn config() -> Configuration {
        Configuration {
            revision: 7,
            enabled: true,
            pixels: 6,
            sources: (0..6).collect(),
            rules: vec![Rule {
                kind: "black".into(),
                pixel: 0,
                offset: 0,
                enabled: true,
            }],
            ..Default::default()
        }
    }
    fn reply(request: &PreviewRequest, lease: u32) -> Configuration {
        let mut value = config();
        value.rules.clear(); // Durable configuration remains the identity map.
        value.active_sources = if request.clear {
            value.sources.clone()
        } else {
            request.value.expected_sources().unwrap()
        };
        value.preview = PreviewLease {
            active: !request.clear,
            lease_id: if request.clear { 0 } else { lease },
            base_revision: request.value.revision,
            remaining_ms: if request.clear { 0 } else { 9500 },
        };
        value
    }
    fn status() -> crate::DeviceStatus {
        serde_json::from_value(
            serde_json::json!({"board_id":"esp32s3-fixture","boot_id":"a".repeat(32),
            "http_peer_fencing":true,"version":crate::peer_identity::VERSION,
            "compatibility":crate::peer_identity::advertised_policy(),"pixels":6,
            "capabilities":{"board_kind":"table","acknowledged_features":["pixel-corrections-preview"]}}),
        )
        .unwrap()
    }
    #[test]
    fn pixel_corrections_cursor_uses_prefix_mapping_not_raw_address() {
        let mut value = config();
        value.rules[0].kind = "remove".into();
        value.rules[0].pixel = 1;
        let cursor = DiagnosticCursor {
            raw: 4,
            revision: 7,
        };
        assert_eq!(value.wire_for_cursor(cursor, 0).unwrap(), 4);
        assert_eq!(value.wire_for_cursor(cursor, 1).unwrap(), 3);
        assert!(
            value
                .wire_for_cursor(DiagnosticCursor { raw: 1, ..cursor }, 1)
                .is_err()
        );
        assert!(
            value
                .wire_for_cursor(
                    DiagnosticCursor {
                        revision: 8,
                        ..cursor
                    },
                    1
                )
                .is_err()
        );
        assert!(
            unique_wire(&[0, 4, 4, -1], cursor.raw)
                .unwrap_err()
                .contains("multiple")
        );
        assert_eq!(value.rules[0].pixel, 1);
    }
    #[test]
    fn pixel_corrections_http_typed_dispatch_is_fenced_and_never_saves() {
        let status = status();
        let request = PreviewRequest {
            id: 31,
            value: config(),
            lease: 0,
            clear: false,
            at: std::time::Instant::now(),
        };
        let actual = reply(&request, 42);
        let (endpoint, server) = crate::tests::mock_device(vec![
            (200, serde_json::to_vec(&status).unwrap()),
            (200, serde_json::to_vec(&actual).unwrap()),
            (200, serde_json::to_vec(&status).unwrap()),
        ]);
        let (tx, _) = std::sync::mpsc::channel();
        let mut queue = crate::peer_http::Queue::new(tx);
        queue.observe(&status);
        let peer = crate::peer_http::Pin::from_status(&status).unwrap();
        let result = crate::scoped_http_request(
            &peer.client().unwrap(),
            &queue.capture(),
            &crate::HttpCommand::CorrectionPreview(endpoint, request),
        );
        assert!(
            matches!(result,Ok(crate::Event::CorrectionPreviewFinished(31,Ok(value))) if value.preview.lease_id==42)
        );
        let requests = server.join().unwrap();
        assert_eq!(requests.len(), 3);
        assert!(requests[1].starts_with("POST /api/pixels/corrections/preview "));
        assert!(requests[1].contains("x-pakar-board: esp32s3-fixture"));
        assert!(requests[1].contains(&format!("x-pakar-boot: {}", "a".repeat(32))));
        let body: serde_json::Value =
            serde_json::from_str(requests[1].split("\r\n\r\n").nth(1).unwrap()).unwrap();
        assert_eq!(body["rules"][0]["pixel"], 0);
        assert_eq!(body["ttl_ms"], PREVIEW_TTL_MS);
        assert!(body.get("save").is_none());
        assert_eq!(
            requests
                .iter()
                .filter(|request| request.starts_with("POST "))
                .count(),
            1
        );
    }
    #[test]
    fn pixel_corrections_http_rejection_preserves_reason_without_replay_or_save() {
        let status = status();
        let request = PreviewRequest {
            id: 31,
            value: config(),
            lease: 0,
            clear: false,
            at: std::time::Instant::now(),
        };
        let (endpoint, server) = crate::tests::mock_device(vec![
            (200, serde_json::to_vec(&status).unwrap()),
            (409, br#"{"reason":"corrections_preview_owned"}"#.to_vec()),
        ]);
        let (tx, _) = std::sync::mpsc::channel();
        let mut queue = crate::peer_http::Queue::new(tx);
        queue.observe(&status);
        let peer = crate::peer_http::Pin::from_status(&status).unwrap();
        let result = crate::scoped_http_request(
            &peer.client().unwrap(),
            &queue.capture(),
            &crate::HttpCommand::CorrectionPreview(endpoint, request),
        );
        assert!(result.err().unwrap().contains("corrections_preview_owned"));
        assert_eq!(server.join().unwrap().len(), 2);
    }
    #[test]
    fn pixel_corrections_http_release_requires_owned_lease_and_same_boot_readback() {
        let status = status();
        let peer = crate::peer_http::Pin::from_status(&status).unwrap();
        let request = PreviewRequest {
            id: 32,
            value: config(),
            lease: 42,
            clear: true,
            at: std::time::Instant::now(),
        };
        let mut next = status.clone();
        next.boot_id = "b".repeat(32);
        let (endpoint, server) = crate::tests::mock_device(vec![
            (200, serde_json::to_vec(&reply(&request, 42)).unwrap()),
            (200, serde_json::to_vec(&next).unwrap()),
        ]);
        let result = send_http_preview(&peer.client().unwrap(), &endpoint, &peer, &request, || {
            Ok(())
        });
        assert!(result.err().unwrap().contains("peer changed"));
        let requests = server.join().unwrap();
        assert_eq!(requests.len(), 2);
        let body: serde_json::Value =
            serde_json::from_str(requests[0].split("\r\n\r\n").nth(1).unwrap()).unwrap();
        assert_eq!(
            body,
            serde_json::json!({"revision":7,"lease_id":42,"clear":true})
        );
    }
    #[test]
    fn pixel_corrections_clear_cannot_confirm_missing_or_inconsistent_saved_map() {
        let request = PreviewRequest {
            id: 1,
            value: config(),
            lease: 42,
            clear: true,
            at: std::time::Instant::now(),
        };
        let mut actual = reply(&request, 42);
        actual.active_sources.clear();
        actual.sources.clear();
        assert!(request.verify(&actual).is_err());
        let mut actual = reply(&request, 42);
        actual.sources[0] = -1;
        actual.active_sources[0] = -1;
        assert!(request.verify(&actual).is_err());
    }
    #[test]
    fn pixel_corrections_human_addresses_round_trip_without_saved_rule_migration() {
        for raw in [0, 1, 5, 6, 478, 479] {
            let mut rule: Rule = serde_json::from_value(
                serde_json::json!({"kind":"remove","pixel":raw,"offset":0,"enabled":false}),
            )
            .unwrap();
            assert_eq!(rule.human_pixel(), raw + 1);
            assert!(!rule.set_human_pixel(raw + 1, 480));
            assert_eq!(serde_json::to_value(&rule).unwrap()["pixel"], raw);
            assert!(!rule.enabled);
        }
        let mut rule = config().rules.remove(0);
        assert!(!rule.set_human_pixel(0, 480));
        assert!(!rule.set_human_pixel(481, 480));
        assert!(rule.set_human_pixel(480, 480));
        assert_eq!(rule.pixel, 479);
    }
    #[test]
    fn pixel_corrections_preview_preference_defaults_on_and_round_trips_without_a_lease() {
        let mut config: crate::config::AppConfig =
            serde_json::from_value(serde_json::json!({})).unwrap();
        assert!(config.pixel_correction_live_preview);
        config.pixel_correction_live_preview = false;
        let json = serde_json::to_value(&config).unwrap();
        assert_eq!(json["pixel_correction_live_preview"], false);
        assert!(json.get("lease_id").is_none());
        assert!(
            !serde_json::from_value::<crate::config::AppConfig>(json)
                .unwrap()
                .pixel_correction_live_preview
        );
    }
    #[test]
    fn pixel_corrections_mapping_matches_shared_ordered_wire_semantics() {
        let mut value = config();
        assert_eq!(value.expected_sources().unwrap(), [-1, 1, 2, 3, 4, 5]);
        value.rules[0].kind = "remove".into();
        value.rules[0].pixel = 1;
        assert_eq!(value.expected_sources().unwrap(), [0, 2, 3, 4, 5, -1]);
        value.rules.push(Rule {
            kind: "shift".into(),
            pixel: 2,
            offset: 1,
            enabled: true,
        });
        assert_eq!(value.expected_sources().unwrap(), [0, 2, -1, 3, 4, 5]);
        value.rules[0].enabled = false;
        value.rules[1].offset = -2;
        assert_eq!(value.expected_sources().unwrap(), [2, 3, 4, 5, -1, -1]);
        value.enabled = false;
        assert_eq!(value.expected_sources().unwrap(), [0, 1, 2, 3, 4, 5]);
    }
    #[test]
    fn pixel_corrections_preview_defaults_on_but_only_edit_acquires_and_never_saves() {
        let now = std::time::Instant::now();
        let mut live = LivePreview::default();
        let value = config();
        assert!(live.enabled);
        assert!(live.next(&value, now).is_none());
        live.edited(now);
        assert!(live.next(&value, now).is_none());
        let sent = now + std::time::Duration::from_millis(300);
        let request = live.next(&value, sent).unwrap();
        assert_eq!(request.payload()["lease_id"], 0);
        assert_eq!(request.payload()["revision"], 7);
        assert_eq!(request.payload()["rules"][0]["pixel"], 0);
        assert!(request.payload().get("save").is_none());
        assert!(live.next(&value, sent).is_none());
        live.finish(request.id, Ok(reply(&request, 42)), sent);
        assert_eq!(live.save_lease(), Some(42));
        assert!(live.engaged);
        assert!(
            live.next(&value, sent + std::time::Duration::from_secs(1))
                .is_none()
        );
        let renewed = live
            .next(&value, sent + std::time::Duration::from_secs(5))
            .unwrap();
        assert_eq!(renewed.lease, 42);
        assert!(!renewed.clear);
        assert_eq!(value.revision, 7);
    }
    #[test]
    fn pixel_corrections_cancelled_acquire_releases_only_returned_owner_once() {
        let now = std::time::Instant::now();
        let mut live = LivePreview::default();
        let value = config();
        live.edited(now);
        let sent = now + std::time::Duration::from_millis(300);
        let request = live.next(&value, sent).unwrap();
        live.stop();
        live.finish(request.id, Ok(reply(&request, 91)), sent);
        let clear = live.next(&value, sent).unwrap();
        assert!(clear.clear);
        assert_eq!(clear.lease, 91);
        live.finish(clear.id, Err("Disconnected".into()), sent);
        assert!(
            live.next(&value, sent + std::time::Duration::from_secs(1))
                .is_none(),
            "Failed clear never retries"
        );
        assert!(!live.engaged);
    }
    #[test]
    fn pixel_corrections_invalid_draft_can_still_release_confirmed_lease() {
        let now = std::time::Instant::now();
        let mut live = LivePreview::default();
        let mut value = config();
        live.edited(now);
        let sent = now + std::time::Duration::from_millis(300);
        let request = live.next(&value, sent).unwrap();
        live.finish(request.id, Ok(reply(&request, 42)), sent);
        value.rules[0].pixel = 6;
        live.stop();
        let clear = live.next(&value, sent).unwrap();
        assert!(clear.clear);
        assert_eq!(clear.lease, 42);
        assert!(clear.value.validate().is_ok());
    }
    #[test]
    fn pixel_corrections_foreign_or_inexact_receipts_cannot_confirm_preview() {
        let now = std::time::Instant::now();
        let value = config();
        let request = PreviewRequest {
            id: 1,
            value: value.clone(),
            lease: 42,
            clear: false,
            at: now,
        };
        for bad in 0..5 {
            let mut actual = reply(&request, 42);
            match bad {
                0 => actual.preview.lease_id = 43,
                1 => actual.revision += 1,
                2 => actual.active_sources[0] = 0,
                3 => actual.preview.remaining_ms = 0,
                _ => actual.preview.base_revision += 1,
            }
            assert!(request.verify(&actual).is_err());
        }
        let mut live = LivePreview::default();
        live.edited(now);
        let request = live
            .next(&value, now + std::time::Duration::from_millis(300))
            .unwrap();
        live.finish(request.id + 1, Ok(reply(&request, 42)), now);
        assert!(live.pending.is_some());
        live.finish(request.id, Err("Uncertain transport".into()), now);
        live.edited(now);
        assert!(
            live.next(&value, now + std::time::Duration::from_secs(1))
                .is_none()
        );
    }
    #[test]
    fn pixel_corrections_preview_expiry_never_reacquires_automatically() {
        let now = std::time::Instant::now();
        let mut live = LivePreview::default();
        let value = config();
        live.edited(now);
        let sent = now + std::time::Duration::from_millis(300);
        let request = live.next(&value, sent).unwrap();
        live.finish(request.id, Ok(reply(&request, 42)), sent);
        assert!(
            live.next(&value, sent + std::time::Duration::from_secs(11))
                .is_none()
        );
        assert_eq!(live.save_lease(), None);
        assert!(!live.engaged);
    }
    #[test]
    fn pixel_corrections_disabled_rules_survive_read_edit_reorder_and_write() {
        let mut value: Configuration =
            serde_json::from_value(serde_json::json!({"revision":7,"enabled":true,"pixels":480,
            "sources":[0,-1,2],"rules":[{"kind":"shift","pixel":1,"offset":-1,"enabled":false},
                {"kind":"black","pixel":9,"offset":0}]}))
            .unwrap();
        assert!(!value.rules[0].enabled);
        assert!(
            value.rules[1].enabled,
            "Omitted old field inherits true only"
        );
        assert!(value.rules[0].step_pixel(1, value.pixels));
        assert!(value.rules[0].step_offset(1, value.pixels));
        value.rules.swap(0, 1);
        let saved = serde_json::to_value(&value).unwrap();
        assert_eq!(saved["rules"][1]["enabled"], false);
        assert_eq!(saved["rules"][1]["pixel"], 2);
        assert_eq!(saved["sources"], serde_json::json!([0, -1, 2]));
        assert_eq!(saved["revision"], 7);
        assert!(
            serde_json::from_value::<Rule>(
                serde_json::json!({"kind":"black","pixel":0,"offset":0,"enabled":"false"})
            )
            .is_err()
        );
    }
    #[test]
    fn pixel_corrections_integer_adjustments_obey_boundaries_without_reenabling() {
        let mut rule = Rule {
            kind: "shift".into(),
            pixel: 0,
            offset: -480,
            enabled: false,
        };
        assert!(!rule.step_pixel(-1, 480));
        assert!(!rule.step_offset(-1, 480));
        assert!(rule.step_pixel(1, 480));
        assert!(rule.step_offset(1, 480));
        rule.pixel = 479;
        rule.offset = 480;
        assert!(!rule.step_pixel(1, 480));
        assert!(!rule.step_offset(1, 480));
        assert!(rule.step_pixel(-1, 480));
        assert!(rule.step_offset(-1, 480));
        assert!(!rule.enabled);
        assert!(!rule.step_pixel(1, 0));
        assert!(!rule.step_offset(1, 0));
        rule.kind = "black".into();
        assert!(!rule.step_offset(-1, 480));
    }
}
