//! Confirmed AVR segment events stay separate from ESP requested presentation.
//! Uses PCController source-generation retirement and selected-peer ownership;
//! no forwarding, tones, command dispatch or capability creation is performed.
use serde::Deserialize;

#[derive(Clone, Debug)]
pub struct Segments {
    board: String,
    boot: String,
    avr_session: u64,
    masks: [u8; 4],
    brightness: u8,
    ack: bool,
}
fn session(status: &crate::DeviceStatus) -> Option<u64> {
    status.mirroring["avr_session"].as_u64().filter(|n| *n > 0)
}
fn ready(status: &crate::DeviceStatus) -> bool {
    status.avr_online
        && status
            .capabilities
            .as_ref()
            .is_some_and(|c| c.has("display"))
        && crate::peer_http::Pin::from_status(status).is_some()
        && session(status).is_some()
}
impl Segments {
    pub fn usb(payload: &[u8], status: &crate::DeviceStatus) -> Option<Self> {
        if payload.len() != 6 || payload[0] != 0x9c || !ready(status) {
            return None;
        }
        Some(Self {
            board: status.board_id.clone(),
            boot: status.boot_id.clone(),
            avr_session: session(status)?,
            masks: payload[1..5].try_into().ok()?,
            brightness: payload[5] & 7,
            ack: payload[5] & 0xc0 == 0xc0,
        })
    }
    pub fn apply(self, status: &mut crate::DeviceStatus) -> bool {
        if !ready(status)
            || self.board != status.board_id
            || self.boot != status.boot_id
            || session(status) != Some(self.avr_session)
        {
            return false;
        }
        status.tm1637_segments = if self.ack {
            self.masks.to_vec()
        } else {
            Vec::new()
        };
        status.tm1637_brightness = self.brightness;
        status.tm1637_ack = self.ack;
        true // No status age, capability, clock, input or cell changes.
    }
}
#[derive(Deserialize)]
pub struct Observation {
    board_id: String,
    boot_id: String,
    avr_session: u64,
    event_id: u32,
    response_opcode: u8,
    payload_hex: String,
}
impl Observation {
    pub fn parse(value: serde_json::Value) -> Option<Self> {
        if value["type"] != "pc-contract-event" {
            return None;
        }
        let event: Self = serde_json::from_value(value).ok()?;
        let payload = crate::pc_contract::decode_hex(&event.payload_hex).ok()?;
        if event.response_opcode == 0x9c && payload.len() != 5 {
            return None;
        }
        Some(event)
    }
}
#[derive(Default)]
pub struct State {
    owner: Option<(crate::peer_http::Pin, u64)>,
    last: Option<u32>,
}
impl State {
    pub fn clear(&mut self) {
        *self = Self::default();
    }
    pub fn observe(&mut self, event: Observation, status: &mut crate::DeviceStatus) -> bool {
        if !ready(status)
            || event.board_id != status.board_id
            || event.boot_id != status.boot_id
            || session(status) != Some(event.avr_session)
        {
            return false;
        }
        let owner = (
            crate::peer_http::Pin::from_status(status).unwrap(),
            event.avr_session,
        );
        if self.owner.as_ref() != Some(&owner) {
            self.clear();
            self.owner = Some(owner);
        }
        if self.last.is_some_and(|last| {
            let delta = event.event_id.wrapping_sub(last);
            delta == 0 || delta > 0x7fffffff
        }) {
            return false;
        }
        // Consume every valid event in this source stream, but only segment
        // frames can change this domain. A missed frame is not replayed.
        self.last = Some(event.event_id);
        if event.response_opcode != 0x9c {
            return false;
        }
        let Ok(mut payload) = crate::pc_contract::decode_hex(&event.payload_hex) else {
            return false;
        };
        payload.insert(0, 0x9c);
        Segments::usb(&payload, status).is_some_and(|frame| frame.apply(status))
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    fn status() -> crate::DeviceStatus {
        serde_json::from_value(serde_json::json!({"board_id":"board-a","boot_id":"a".repeat(32),"http_peer_fencing":true,
            "avr_online":true,"mirroring":{"avr_session":1},"command_clock_ms":42,"cells":["red"],
            "capabilities":{"board_kind":"controller","acknowledged_features":["display"]}})).unwrap()
    }
    fn event(id: u32) -> Observation {
        Observation::parse(serde_json::json!({"type":"pc-contract-event","board_id":"board-a","boot_id":"a".repeat(32),
            "avr_session":1,"event_id":id,"response_opcode":156,"payload_hex":"01020408c7"})).unwrap()
    }
    #[test]
    fn raw_segment_events_require_current_peer_avr_session_and_ack_without_renewing_other_state() {
        let mut status = status();
        let mut state = State::default();
        assert!(state.observe(event(9), &mut status));
        assert_eq!(status.tm1637_segments, [1, 2, 4, 8]);
        assert!(status.tm1637_ack);
        assert_eq!(status.tm1637_brightness, 7);
        assert_eq!(status.command_clock_ms, Some(42));
        assert_eq!(status.cells, ["red"]);
        assert!(!state.observe(event(9), &mut status));
        let mut foreign = event(10);
        foreign.avr_session = 2;
        assert!(!state.observe(foreign, &mut status));
        let mut no_ack = event(10);
        no_ack.payload_hex = "0102040807".into();
        assert!(state.observe(no_ack, &mut status));
        assert!(status.tm1637_segments.is_empty());
        assert!(!status.tm1637_ack);
        status.avr_online = false;
        assert!(Segments::usb(&[0x9c, 1, 2, 3, 4, 0xc7], &status).is_none());
    }
    #[test]
    fn malformed_and_unsolicited_non_segment_frames_do_not_infer_hardware() {
        let mut status = status();
        let mut state = State::default();
        let mut other = event(1);
        other.response_opcode = 0x9d;
        assert!(!state.observe(other, &mut status));
        assert!(status.tm1637_segments.is_empty());
        assert!(Segments::usb(&[0x9c, 1, 2, 3, 4], &status).is_none());
        let mut wrong = event(2);
        wrong.boot_id = "b".repeat(32);
        assert!(!state.observe(wrong, &mut status));
    }
}
