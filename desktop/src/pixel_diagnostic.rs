//! Deliberate one-frame diagnostics, not an animation engine or a replay queue.
use crate::{DeviceStatus, Endpoint, peer_http::Pin};
use reqwest::blocking::Client;
use std::time::{Duration, Instant};

#[derive(Clone, Copy, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[serde(default)]
pub struct Probe {
    pub index: u16,
    pub group: bool,
    pub rgb: u32,
    #[serde(default = "table_pixels")]
    pub pixels: u16,
    #[serde(default = "table_group")]
    pub group_pixels: u8,
    pub group_start: Option<u16>,
    pub group_layout_revision: Option<u32>,
    pub used_pixels: Option<u16>,
}
const fn table_pixels() -> u16 {
    480
}
const fn table_group() -> u8 {
    6
}
impl Default for Probe {
    fn default() -> Self {
        Self {
            index: 0,
            group: false,
            rgb: 0xffffff,
            pixels: 480,
            group_pixels: 6,
            group_start: None,
            group_layout_revision: None,
            used_pixels: None,
        }
    }
}
pub fn profile(s: &DeviceStatus) -> Option<(u16, u8)> {
    let role = s.capabilities.as_ref()?.role.as_str();
    match (role, s.pixels) {
        ("first-wins", 216) => Some((216, 12)),
        ("table-selection" | "", 480) => Some((480, 6)),
        _ => None,
    }
}
impl Probe {
    pub fn for_status(s: &DeviceStatus, index: u16, group: bool, rgb: u32) -> Option<Self> {
        let layout = crate::pixel_groups::for_status(s)?;
        let physical = layout.containing(index)?;
        let (start, count) = layout.span(physical)?;
        let advertised = crate::pixel_groups::advertised(s);
        Some(Self { index, group, rgb, pixels: layout.capacity, group_pixels: count,
            group_start: advertised.then_some(start), group_layout_revision: advertised.then_some(layout.revision),
            used_pixels: advertised.then_some(layout.used_pixels) }).filter(|p| p.valid())
    }
    pub fn from_form(s: &DeviceStatus, form: &[(String, String)]) -> Option<(Self, bool)> {
        let get = |name| form.iter().find(|(key, _)| key == name).map(|(_, value)| value.as_str());
        let count = get("count")?.parse::<u8>().ok()?;
        let probe = Self::for_status(s, get("index")?.parse().ok()?, count > 1, get("rgb")?.parse().ok()?)?;
        if probe.count() != u16::from(count) || probe.first() != probe.index
            || probe.group_layout_revision != get("group_layout_revision").map(str::parse::<u32>).transpose().ok()? { return None; }
        Some((probe, match get("off")? { "true" => true, "false" => false, _ => return None }))
    }
    pub fn moved_on(self, direction: i16, layout: &crate::pixel_groups::Layout) -> Option<Self> {
        if !matches!(direction, -1 | 1) { return None; }
        let index = if self.group {
            let group = i16::from(layout.containing(self.index)?) + direction;
            layout.span(u8::try_from(group).ok()?)?.0
        } else { u16::try_from(i32::from(self.index) + i32::from(direction)).ok()? };
        let (start, count) = layout.span(layout.containing(index)?)?;
        Some(Self { index, group_start: self.group_layout_revision.map(|_| start), group_pixels: count, ..self })
    }
    pub fn usb(self, off: bool) -> Option<Vec<u8>> {
        if !self.valid() || self.group_layout_revision.is_some() {
            return None;
        }
        let mut p = self.first().to_le_bytes().to_vec();
        p.extend([
            self.count() as u8,
            (self.rgb >> 16) as u8,
            (self.rgb >> 8) as u8,
            self.rgb as u8,
            u8::from(off),
        ]);
        Some(p)
    }
    pub fn from_usb(p: &[u8]) -> Option<(Self, bool)> {
        Self::from_usb_profile(p, 480, 6)
    }
    pub fn from_usb_profile(p: &[u8], pixels: u16, group_pixels: u8) -> Option<(Self, bool)> {
        if p.len() != 7 || !matches!(p[2], 1 | 6 | 12) || p[6] > 1 {
            return None;
        }
        let probe = Self {
            index: u16::from_le_bytes([p[0], p[1]]),
            group: p[2] != 1,
            rgb: u32::from(p[3]) << 16 | u32::from(p[4]) << 8 | u32::from(p[5]),
            pixels,
            group_pixels,
            ..Default::default()
        };
        (probe.valid()
            && probe.count() == u16::from(p[2])
            && probe.first() == probe.index
            && probe.first() + probe.count() <= pixels)
            .then_some((probe, p[6] != 0))
    }
    pub fn count(self) -> u16 {
        if self.group {
            u16::from(self.group_pixels)
        } else {
            1
        }
    }
    pub fn first(self) -> u16 {
        if self.group {
            self.group_start.unwrap_or_else(|| self.index / u16::from(self.group_pixels.max(1)) * u16::from(self.group_pixels))
        } else {
            self.index
        }
    }
    pub fn valid(self) -> bool {
        ((self.group_layout_revision.is_none() && matches!((self.pixels, self.group_pixels), (480, 6) | (216, 12)))
            || (self.group_layout_revision.is_some() && matches!(self.pixels, 480 | 216)
                && self.group_pixels > 0 && self.group_start.is_some()
                && self.used_pixels.is_some_and(|used| used > 0 && used <= self.pixels)))
            && self.index < self.used_pixels.unwrap_or(self.pixels)
            && self.first().checked_add(self.count()).is_some_and(|end| end <= self.used_pixels.unwrap_or(self.pixels))
            && self.group_start.is_none_or(|start| self.index >= start && self.index < start + u16::from(self.group_pixels))
            && self.rgb <= 0xffffff
    }
    pub fn moved(self, direction: i16) -> Option<Self> {
        if self.group_layout_revision.is_some() { return None; } // Use the board's adjacent group, not a guessed fixed stride.
        let index = self.first() as i16 + direction * self.count() as i16;
        (self.valid()
            && matches!(direction, -1 | 1)
            && index >= 0
            && index + self.count() as i16 <= self.pixels as i16)
            .then_some(Self {
                index: index as u16,
                ..self
            })
    }
    pub fn label(self) -> String {
        if self.group {
            format!(
                "Pixels {}–{}",
                self.first() + 1,
                self.first() + self.count()
            )
        } else {
            format!("Pixel {}", self.first() + 1)
        }
    }
    pub fn form(self, off: bool) -> Vec<(String, String)> {
        let mut form = vec![
            ("index".into(), self.first().to_string()),
            ("count".into(), self.count().to_string()),
            ("rgb".into(), self.rgb.to_string()),
            ("off".into(), off.to_string()),
        ];
        if let Some(revision) = self.group_layout_revision { form.push(("group_layout_revision".into(), revision.to_string())); }
        form
    }
    pub fn frame(self, off: bool) -> Option<Vec<u8>> {
        if !self.valid() {
            return None;
        }
        let mut bytes = vec![0; usize::from(self.pixels) * 3];
        if !off {
            for index in self.first()..self.first() + self.count() {
                bytes[index as usize * 3..index as usize * 3 + 3].copy_from_slice(&[
                    (self.rgb >> 16) as u8,
                    (self.rgb >> 8) as u8,
                    self.rgb as u8,
                ]);
            }
        }
        Some(bytes)
    }
    pub fn matches(self, status: &DeviceStatus, off: bool) -> bool {
        let Some(frame) = status.pixel_frame.as_ref().filter(|f| f.valid()) else {
            return false;
        };
        if self.group_layout_revision.is_some() && (frame.group_layout_revision != self.group_layout_revision
            || crate::pixel_groups::for_status(status).is_none_or(|l| Some(l.revision) != self.group_layout_revision)) { return false; }
        self.frame(off).is_some_and(|expected| {
            frame.rgb.len() == expected.len() * 2
                && frame
                    .rgb
                    .as_bytes()
                    .chunks_exact(2)
                    .zip(expected)
                    .all(|(hex, byte)| {
                        std::str::from_utf8(hex)
                            .ok()
                            .and_then(|v| u8::from_str_radix(v, 16).ok())
                            == Some(byte)
                    })
        })
    }
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Observation {
    pub peer: Pin,
    pub revision: u32,
    pub active: bool,
    pub probe: Probe,
    pub off: bool,
}
impl Observation {
    pub fn read(s: &DeviceStatus) -> Option<Self> {
        if !supported(s) || !s.capabilities.as_ref()?.has("pixel-diagnostic") {
            return None;
        }
        let v = &s.led_output["diagnostic"];
        let revision = u32::try_from(v["revision"].as_u64()?).ok()?;
        let index = u16::try_from(v["index"].as_u64()?).ok()?;
        let count = v["count"].as_u64()?;
        let probe = Probe::for_status(s, index, count > 1, u32::try_from(v["rgb"].as_u64()?).ok()?)?;
        if !probe.valid()
            || probe.count() != count as u16
            || probe.first() != index
            || (probe.group_layout_revision.is_some() && v["group_layout_revision"].as_u64() != probe.group_layout_revision.map(u64::from))
        {
            return None;
        }
        let active = v["active"].as_bool()?;
        if active && revision == 0 {
            return None;
        }
        Some(Self {
            peer: Pin::from_status(s)?,
            revision,
            active,
            probe,
            off: v["off"].as_bool()?,
        })
    }
    pub fn label(&self) -> String {
        if !self.active {
            "Board diagnostic inactive".into()
        } else if self.off {
            format!("Board reports: all {} pixels off", self.probe.pixels)
        } else {
            format!(
                "Board reports: {} · #{:06X}",
                self.probe.label(),
                self.probe.rgb
            )
        }
    }
}

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
    pub group: bool,
    pub rgb: Option<u32>,
    pub pixels: u16,
    pub group_pixels: u8,
    pub layout: Option<crate::pixel_groups::Layout>,
    pub custom_layout: bool,
    pub observed: Option<Observation>,
    pub pending: Option<(u64, Instant)>,
    pub armed: Option<Pin>,
    pub message: String,
    next_id: u64,
}
impl State {
    pub fn probe(&self) -> Probe {
        let pixels = if self.pixels == 216 { 216 } else { 480 };
        let group_pixels = if pixels == 216 { 12 } else { 6 };
        let mut probe = Probe {
            index: self.index.clamp(1, pixels) - 1,
            group: self.group,
            rgb: self.rgb.unwrap_or(0xffffff),
            pixels,
            group_pixels,
            ..Default::default()
        };
        if self.custom_layout && let Some(layout) = &self.layout {
            probe.index = self.index.clamp(1, layout.used_pixels) - 1;
            if let Some((start, count)) = layout.containing(probe.index).and_then(|g| layout.span(g)) {
                probe.group_pixels = count; probe.group_start = Some(start);
                probe.group_layout_revision = Some(layout.revision); probe.used_pixels = Some(layout.used_pixels);
            }
        }
        probe
    }
    pub fn set_probe(&mut self, probe: Probe) {
        self.index = probe.first() + 1;
        self.group = probe.group;
        self.rgb = Some(probe.rgb);
        self.pixels = probe.pixels;
        self.group_pixels = probe.group_pixels;
    }
    pub fn observe(&mut self, status: &DeviceStatus) -> bool {
        self.layout = crate::pixel_groups::for_status(status);
        self.custom_layout = crate::pixel_groups::advertised(status);
        if let Some((pixels, group_pixels)) = profile(status) {
            self.pixels = pixels;
            self.group_pixels = group_pixels;
            self.index = self.index.clamp(1, pixels);
        }
        let Some(next) = Observation::read(status) else {
            return false;
        };
        if self
            .observed
            .as_ref()
            .is_some_and(|old| old.peer == next.peer && next.revision <= old.revision)
        {
            return false;
        }
        if next.active && !next.off {
            self.set_probe(next.probe);
        }
        self.observed = Some(next);
        true
    }
    pub fn begin(&mut self) -> u64 {
        self.next_id = self.next_id.wrapping_add(1).max(1);
        self.pending = Some((self.next_id, Instant::now()));
        self.next_id
    }
    pub fn matches(&self, id: u64) -> bool {
        self.pending.is_some_and(|(active, _)| active == id)
    }
    pub fn finish(&mut self, id: u64, message: String) {
        if self.matches(id) {
            self.pending = None;
            self.message = message;
        }
    }
    pub fn revoke(&mut self) {
        self.pending = None;
        self.armed = None;
        self.observed = None;
        self.message.clear();
    }
    pub fn expire(&mut self) {
        if self
            .pending
            .is_some_and(|(_, at)| at.elapsed() >= Duration::from_secs(12))
        {
            self.pending = None;
            self.message = "Diagnostic outcome unavailable; nothing was replayed. Next is a new deliberate frame.".into();
        }
    }
}
pub fn supported(s: &DeviceStatus) -> bool {
    profile(s).is_some()
        && crate::pixel_groups::for_status(s).is_some()
        && Pin::from_status(s).is_some()
        && s.capabilities
            .as_ref()
            .is_some_and(|m| m.has("leds") && (m.has("streaming") || m.has("pixel-diagnostic")))
}
pub fn usb_supported(s: &DeviceStatus) -> bool {
    supported(s)
        && s.capabilities.as_ref().is_some_and(|c| {
            (c.has("usb-pixel-diagnostic") || c.has("usb-api")) && c.has("usb-demand") && c.has("pixel-diagnostic")
                && (!crate::pixel_groups::advertised(s) || c.has("usb-api"))
        })
}
pub fn usb_verify(s: &DeviceStatus, p: &[u8]) -> bool {
    let Some((pixels, group_pixels)) = profile(s) else {
        return false;
    };
    let Some((probe, off)) = Probe::from_usb_profile(p, pixels, group_pixels) else {
        return false;
    };
    let Some(observed) = Observation::read(s) else {
        return false;
    };
    usb_supported(s)
        && observed.active
        && observed.revision > 0
        && observed.probe == probe
        && observed.off == off
        && probe.matches(s, off)
}
pub fn api_command(status: &DeviceStatus, probe: Probe, off: bool) -> Result<(String, serde_json::Value), String> {
    let (current, _) = Probe::from_form(status, &probe.form(off)).ok_or("Physical group layout changed; nothing sent")?;
    if current.first()!=probe.first() || current.count()!=probe.count() || current.rgb!=probe.rgb
        || current.group_layout_revision!=probe.group_layout_revision { return Err("Diagnostic layout is stale; nothing sent".into()); }
    let id = uuid::Uuid::new_v4().simple().to_string();
    let mut body = serde_json::json!({"type":"command","command":"pixel-diagnostic","request_id":id,
        "board_id":status.board_id,"boot_id":status.boot_id,"expires_at_ms":status.command_clock_ms
            .and_then(|v|v.checked_add(8000)).ok_or("Board command clock unavailable")?,
        "index":probe.first(),"count":probe.count(),"rgb":probe.rgb,"off":off});
    if let Some(revision) = probe.group_layout_revision { body["group_layout_revision"] = revision.into(); }
    Ok((id,body))
}
pub fn verify_api_receipt(before: &DeviceStatus, value: &serde_json::Value, request_id: &str, probe: Probe, off: bool) -> Result<Observation,String> {
    let peer = Pin::from_status(before).ok_or("USB peer unavailable")?;
    if value["request_id"] != request_id || value["accepted"] != true
        || !peer.matches_identity(value["board_id"].as_str().unwrap_or(""), value["boot_id"].as_str().unwrap_or("")) {
        return Err(format!("USB diagnostic not confirmed: {}", value["error"].as_str().unwrap_or("receipt identity/acceptance mismatch")));
    }
    let fragment = &value["status"];
    if !peer.matches_identity(fragment["board_id"].as_str().unwrap_or(""), fragment["boot_id"].as_str().unwrap_or("")) {
        return Err("USB diagnostic readback missing or peer changed; no replay".into());
    }
    let mut merged = if value["status_partial"] == true { serde_json::to_value(before).map_err(|e|e.to_string())? } else { serde_json::json!({}) };
    for (key, field) in fragment.as_object().ok_or("Invalid diagnostic readback")? { merged[key] = field.clone(); }
    // Compact output metadata is confirmation, not RGB. Never retain old RGB.
    let frame = &fragment["pixel_frame"];
    merged["pixel_frame"] = serde_json::Value::Null;
    let status: DeviceStatus = serde_json::from_value(merged).map_err(|_|"Invalid diagnostic state")?;
    let observed = Observation::read(&status).ok_or("Diagnostic specification missing or invalid")?;
    let previous = before.led_output["diagnostic"]["revision"].as_u64().unwrap_or(0);
    if observed.probe.first()!=probe.first() || observed.probe.count()!=probe.count() || observed.probe.rgb!=probe.rgb
        || observed.probe.group_layout_revision!=probe.group_layout_revision
        || !observed.active || observed.off != off || u64::from(observed.revision) <= previous
        || frame["pending"] != false || frame["revision"].as_u64().is_none_or(|rev|
            rev <= before.pixel_frame.as_ref().map_or(0, |f| u64::from(f.revision))) {
        return Err("Diagnostic accepted but publication/readback differs; no replay".into());
    }
    Ok(observed)
}
pub fn ready(s: &DeviceStatus) -> bool {
    can_arm(s) && crate::led_output_mode(s) == Some("effects")
}
pub fn can_arm(s: &DeviceStatus) -> bool {
    supported(s)
        && !s.programming_latch
        && matches!(s.mode.as_str(), "cell" | "first-wins")
        && s.effect == "none"
        && s.remote_firmware.get("busy").and_then(|v| v.as_bool()) != Some(true)
        && s.led_output.get("enabled").and_then(|v| v.as_bool()) == Some(true)
}
/// Only the exact led-output state response can acknowledge an arm request.
pub fn mode_ack(text: &str) -> Option<Result<(), String>> {
    let value: serde_json::Value = serde_json::from_str(text).ok()?;
    let accepted = value.get("accepted")?.as_bool()?;
    let message = value.get("message")?.as_str()?;
    if !accepted {
        return Some(Err(
            "Device rejected diagnostic effects mode; no frame sent".into(),
        ));
    }
    let state: serde_json::Value = serde_json::from_str(message).ok()?;
    if state.get("mode").and_then(|v| v.as_str()) != Some("effects")
        || state.get("enabled").and_then(|v| v.as_bool()) != Some(true)
    {
        return None;
    }
    Some(Ok(()))
}
pub fn pixels(index: u16) -> Option<Vec<u8>> {
    if index >= 480 {
        return None;
    }
    let mut rgb = vec![0; 480 * 3];
    rgb[index as usize * 3..index as usize * 3 + 3].fill(255);
    Some(rgb)
}
fn read_status(client: &Client, endpoint: &Endpoint, pixels: bool) -> Result<DeviceStatus, String> {
    client
        .get(format!(
            "{}/api/status{}",
            endpoint.base.trim_end_matches('/'),
            if pixels { "?pixels=1" } else { "" }
        ))
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .send()
        .map_err(crate::diagnostics::describe_error)?
        .error_for_status()
        .map_err(crate::diagnostics::describe_error)?
        .json()
        .map_err(crate::diagnostics::describe_error)
}
pub fn verify_frame(
    client: &Client,
    endpoint: &Endpoint,
    peer: &Pin,
    index: u16,
) -> Result<String, String> {
    let status = read_status(client, endpoint, true)?;
    if Pin::from_status(&status).as_ref() != Some(peer) || !ready(&status) {
        return Err("Board or diagnostic mode changed before pixel readback".into());
    }
    let frame = status
        .pixel_frame
        .as_ref()
        .filter(|frame| frame.valid())
        .ok_or("Pixel snapshot unavailable")?;
    let expected = pixels(index).ok_or("Pixel index outside 0..479")?;
    if !frame
        .rgb
        .as_bytes()
        .chunks_exact(2)
        .zip(expected)
        .all(|(hex, byte)| {
            std::str::from_utf8(hex)
                .ok()
                .and_then(|v| u8::from_str_radix(v, 16).ok())
                == Some(byte)
        })
    {
        return Err("Pixel snapshot differs from the sent frame; not replayed".into());
    }
    Ok(format!(
        "Device confirmed pixel {} color",
        index + 1
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn pixel_groups_diagnostic_steps_prefix_spans_and_fences_stale_geometry() {
        let mut status=observation(1);
        status.capabilities.as_mut().unwrap().acknowledged_features.push(crate::pixel_groups::FEATURE.into());
        let mut layout=crate::pixel_groups::Layout::uniform(480,6).unwrap();layout.revision=4;
        layout.set_override(1,5);layout.set_override(2,7);
        layout.counts[0]=5;layout.counts[1]=7;layout.starts[1]=5;
        status.pixel_groups=serde_json::to_value(&layout).unwrap();
        let probe=Probe::for_status(&status,3,true,0x123456).unwrap();
        assert_eq!((probe.first(),probe.count()),(0,5));
        let next=probe.moved_on(1,&layout).unwrap();assert_eq!((next.first(),next.count()),(5,7));
        assert_eq!(next.moved_on(1,&layout).unwrap().first(),12);
        assert_eq!(next.moved_on(-1,&layout).unwrap().first(),0);
        assert!(probe.usb(false).is_none(),"variable geometry must use fenced JSON USB API");
        assert!(Probe::from_form(&status,&next.form(false)).is_some());
        status.pixel_groups["revision"]=5.into();
        assert!(Probe::from_form(&status,&next.form(false)).is_none());
        status.pixel_groups=serde_json::Value::Null;
        assert!(!supported(&status),"advertised layout cannot silently revert to six");
    }
    fn observation(revision: u32) -> DeviceStatus {
        serde_json::from_value(serde_json::json!({"board_id":"fixture","boot_id":"a".repeat(32),
            "http_peer_fencing":true,"pixels":480,"capabilities":{"board_kind":"table","acknowledged_features":["leds","pixel-diagnostic"]},
            "led_output":{"diagnostic":{"revision":revision,"active":true,"index":474,"count":6,"rgb":0x1234ab,"off":false}}})).unwrap()
    }
    #[test]
    fn pixel_diagnostic_colours_groups_tail_and_all_off_are_exact() {
        for index in 0..480 {
            for group in [false, true] {
                let probe = Probe {
                    index,
                    group,
                    rgb: 0x1234ab,
                    ..Default::default()
                };
                let frame = probe.frame(false).unwrap();
                for (at, pixel) in frame.chunks_exact(3).enumerate() {
                    assert_eq!(
                        pixel,
                        if (probe.first() as usize..(probe.first() + probe.count()) as usize)
                            .contains(&at)
                        {
                            &[0x12, 0x34, 0xab][..]
                        } else {
                            &[0, 0, 0][..]
                        }
                    );
                }
                assert!(probe.frame(true).unwrap().iter().all(|v| *v == 0));
            }
        }
        assert!(Probe::default().moved(-1).is_none());
        assert!(Probe::default().moved(0).is_none());
        assert!(
            Probe {
                index: 479,
                group: true,
                ..Default::default()
            }
            .moved(1)
            .is_none()
        );
        assert!(
            Probe {
                index: 480,
                ..Default::default()
            }
            .frame(false)
            .is_none()
        );
    }
    #[test]
    fn usb_demand_pixel_command_validates_tail_group_and_exact_published_rgb() {
        let probe = Probe {
            index: 474,
            group: true,
            rgb: 0x1234ab,
            ..Default::default()
        };
        let payload = probe.usb(false).unwrap();
        assert_eq!(payload, vec![0xda, 1, 6, 0x12, 0x34, 0xab, 0]);
        assert_eq!(Probe::from_usb(&payload), Some((probe, false)));
        for p in [
            vec![0xdf, 1, 6, 1, 2, 3, 0],
            vec![0, 0, 2, 1, 2, 3, 0],
            vec![0, 0, 1, 1, 2, 3, 2],
        ] {
            assert!(Probe::from_usb(&p).is_none());
        }
        let mut status = observation(3);
        status
            .capabilities
            .as_mut()
            .unwrap()
            .acknowledged_features
            .extend(["usb-demand".into(), "usb-pixel-diagnostic".into()]);
        assert!(
            !usb_verify(&status, &payload),
            "An ACK/metadata-only observation is not RGB verification"
        );
        let hex = probe
            .frame(false)
            .unwrap()
            .iter()
            .map(|v| format!("{v:02x}"))
            .collect::<String>();
        status.pixel_frame=Some(serde_json::from_value(serde_json::json!({"rgb":hex,"physical":(1..=80).collect::<Vec<_>>(),"revision":12,"brightness":255})).unwrap());
        assert!(usb_verify(&status, &payload));
        status.pixel_frame.as_mut().unwrap().pending = true;
        assert!(!usb_verify(&status, &payload));
        status.pixel_frame.as_mut().unwrap().pending = false;
        status
            .pixel_frame
            .as_mut()
            .unwrap()
            .rgb
            .replace_range(0..2, "ff");
        assert!(!usb_verify(&status, &payload));
    }
    #[test]
    fn pixel_diagnostic_remote_observation_is_ordered_and_never_actuates() {
        let mut state = State::default();
        let current = observation(3);
        assert!(state.observe(&current));
        assert_eq!(state.probe().first(), 474);
        assert!(state.pending.is_none() && state.armed.is_none());
        state.set_probe(Probe::default());
        assert!(!state.observe(&current));
        assert!(!state.observe(&observation(2)));
        assert_eq!(state.probe(), Probe::default()); // Duplicate push cannot replace a draft.
        let mut off = observation(4);
        off.led_output["diagnostic"]["off"] = true.into();
        assert!(state.observe(&off));
        assert_eq!(state.probe(), Probe::default());
        assert!(
            state
                .observed
                .as_ref()
                .unwrap()
                .label()
                .contains("all 480 pixels off")
        );
        let mut invalid = observation(5);
        invalid.led_output["diagnostic"]["index"] = 479.into();
        assert!(!state.observe(&invalid));
        let mut inactive = observation(5);
        inactive.led_output["diagnostic"]["active"] = false.into();
        assert!(state.observe(&inactive));
        assert!(!state.observed.as_ref().unwrap().active);
        state.revoke();
        assert!(state.observed.is_none());
        assert_eq!(state.probe(), Probe::default());
    }
    #[test]
    fn each_manual_frame_has_only_one_white_pixel_and_all_others_black() {
        for index in [0, 1, 5, 6, 239, 240, 478, 479] {
            let rgb = pixels(index).unwrap();
            assert_eq!(rgb.len(), 1440);
            for (at, pixel) in rgb.chunks_exact(3).enumerate() {
                assert_eq!(
                    pixel,
                    if at == index as usize {
                        &[255, 255, 255][..]
                    } else {
                        &[0, 0, 0][..]
                    }
                );
            }
        }
        assert!(pixels(480).is_none());
    }
    #[test]
    fn late_results_do_not_finish_a_new_manual_request_and_no_auto_advance() {
        let mut s = State {
            index: 17,
            ..Default::default()
        };
        let old = s.begin();
        let current = s.begin();
        s.finish(old, "late".into());
        assert!(s.matches(current));
        s.finish(current, "sent".into());
        assert!(s.pending.is_none());
        assert_eq!(s.index, 17);
        s.armed = None;
        s.revoke();
        assert!(s.pending.is_none());
    }
    #[test]
    fn only_exact_output_ack_can_arm_not_pong_or_unrelated_acceptance() {
        for text in [
            r#"{"type":"pong"}"#,
            r#"{"accepted":true}"#,
            r#"{"accepted":true,"message":"{}"}"#,
            r#"{"accepted":true,"message":"{\"mode\":\"static\",\"enabled\":true}"}"#,
        ] {
            assert!(mode_ack(text).is_none());
        }
        assert!(
            mode_ack(r#"{"accepted":true,"message":"{\"mode\":\"effects\",\"enabled\":true}"}"#)
                .unwrap()
                .is_ok()
        );
    }
}
