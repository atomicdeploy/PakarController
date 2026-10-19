//! Deliberate one-frame diagnostics, not an animation engine or a replay queue.
use crate::{DeviceStatus, Endpoint, peer_http::Pin};
use reqwest::blocking::Client;
use std::time::{Duration, Instant};

#[derive(Clone, Copy, Debug, PartialEq, Eq,serde::Serialize,serde::Deserialize)]
#[serde(default)]
pub struct Probe { pub index: u16, pub group: bool, pub rgb: u32 }
impl Default for Probe { fn default()->Self { Self{index:0,group:false,rgb:0xffffff} } }
impl Probe {
    pub fn count(self)->u16 {if self.group {6}else{1}}
    pub fn first(self)->u16 {if self.group {self.index/6*6}else{self.index}}
    pub fn valid(self)->bool {self.index<480 && self.rgb<=0xffffff}
    pub fn moved(self,direction:i16)->Option<Self> {
        let index=self.first() as i16+direction*self.count() as i16;
        (self.valid() && matches!(direction,-1|1) && index>=0 && index+self.count() as i16<=480)
            .then_some(Self{index:index as u16,..self})
    }
    pub fn label(self)->String {if self.group {format!("Pixels {}–{}",self.first()+1,self.first()+6)}else{format!("Pixel {}",self.first()+1)}}
    pub fn form(self,off:bool)->Vec<(String,String)> {vec![("index".into(),self.first().to_string()),
        ("count".into(),self.count().to_string()),("rgb".into(),self.rgb.to_string()),("off".into(),off.to_string())]}
    pub fn frame(self,off:bool)->Option<Vec<u8>> {
        if !self.valid(){return None}let mut bytes=vec![0;1440];
        if !off {for index in self.first()..self.first()+self.count() {bytes[index as usize*3..index as usize*3+3]
            .copy_from_slice(&[(self.rgb>>16)as u8,(self.rgb>>8)as u8,self.rgb as u8]);}}
        Some(bytes)
    }
    pub fn matches(self,status:&DeviceStatus,off:bool)->bool {
        let Some(frame)=status.pixel_frame.as_ref().filter(|f|f.valid())else{return false};
        self.frame(off).is_some_and(|expected|frame.rgb.as_bytes().chunks_exact(2).zip(expected)
            .all(|(hex,byte)|std::str::from_utf8(hex).ok().and_then(|v|u8::from_str_radix(v,16).ok())==Some(byte)))
    }
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Observation {pub peer:Pin,pub revision:u32,pub active:bool,pub probe:Probe,pub off:bool}
impl Observation {
    pub fn read(s:&DeviceStatus)->Option<Self> {
        if !supported(s) || !s.capabilities.as_ref()?.has("pixel-diagnostic"){return None}let v=&s.led_output["diagnostic"];
        let revision=u32::try_from(v["revision"].as_u64()?).ok()?;
        let index=u16::try_from(v["index"].as_u64()?).ok()?;let count=v["count"].as_u64()?;
        let probe=Probe{index,group:count==6,rgb:u32::try_from(v["rgb"].as_u64()?).ok()?};
        if !probe.valid() || !matches!(count,1|6) || index+count as u16>480 || (count==6 && index%6!=0){return None}
        let active=v["active"].as_bool()?;if active && revision==0{return None}
        Some(Self{peer:Pin::from_status(s)?,revision,active,probe,off:v["off"].as_bool()?})
    }
    pub fn label(&self)->String {if !self.active {"Board diagnostic inactive".into()}else if self.off {"Board reports: all 480 pixels off".into()}
        else {format!("Board reports: {} · #{:06X}",self.probe.label(),self.probe.rgb)}}
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
    pub observed: Option<Observation>,
    pub pending: Option<(u64, Instant)>,
    pub armed: Option<Pin>,
    pub message: String,
    next_id: u64,
}
impl State {
    pub fn probe(&self)->Probe {Probe{index:self.index.clamp(1,480)-1,group:self.group,rgb:self.rgb.unwrap_or(0xffffff)}}
    pub fn set_probe(&mut self,probe:Probe){self.index=probe.first()+1;self.group=probe.group;self.rgb=Some(probe.rgb);}
    pub fn observe(&mut self,status:&DeviceStatus)->bool {
        let Some(next)=Observation::read(status)else{return false};
        if self.observed.as_ref().is_some_and(|old|old.peer==next.peer && next.revision<=old.revision){return false}
        if next.active && !next.off {self.set_probe(next.probe);}
        self.observed=Some(next);true
    }
    pub fn begin(&mut self) -> u64 {
        self.next_id = self.next_id.wrapping_add(1).max(1);
        self.pending = Some((self.next_id, Instant::now()));
        self.next_id
    }
    pub fn matches(&self, id: u64) -> bool { self.pending.is_some_and(|(active, _)| active == id) }
    pub fn finish(&mut self, id: u64, message: String) {
        if self.matches(id) { self.pending = None; self.message = message; }
    }
    pub fn revoke(&mut self) { self.pending = None; self.armed = None; self.observed=None;self.message.clear(); }
    pub fn expire(&mut self) {
        if self.pending.is_some_and(|(_, at)| at.elapsed() >= Duration::from_secs(12)) {
            self.pending = None;
            self.message = "Diagnostic outcome unavailable; nothing was replayed. Next is a new deliberate frame.".into();
        }
    }
}
pub fn supported(s: &DeviceStatus) -> bool {
    s.pixels == 480 && Pin::from_status(s).is_some()
        && s.capabilities.as_ref().is_some_and(|m| m.has("leds") && (m.has("streaming") || m.has("pixel-diagnostic")))
}
pub fn ready(s: &DeviceStatus) -> bool {
    can_arm(s) && crate::led_output_mode(s) == Some("effects")
}
pub fn can_arm(s: &DeviceStatus) -> bool {
    supported(s) && !s.programming_latch && s.mode == "cell" && s.effect == "none"
        && s.remote_firmware.get("busy").and_then(|v|v.as_bool()) != Some(true)
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
    fn observation(revision:u32)->DeviceStatus {
        serde_json::from_value(serde_json::json!({"board_id":"fixture","boot_id":"a".repeat(32),
            "http_peer_fencing":true,"pixels":480,"capabilities":{"board_kind":"table","acknowledged_features":["leds","pixel-diagnostic"]},
            "led_output":{"diagnostic":{"revision":revision,"active":true,"index":474,"count":6,"rgb":0x1234ab,"off":false}}})).unwrap()
    }
    #[test]
    fn pixel_diagnostic_colours_groups_tail_and_all_off_are_exact() {
        for index in 0..480 {for group in [false,true] {
            let probe=Probe{index,group,rgb:0x1234ab};let frame=probe.frame(false).unwrap();
            for (at,pixel) in frame.chunks_exact(3).enumerate() {
                assert_eq!(pixel,if (probe.first() as usize..(probe.first()+probe.count())as usize).contains(&at){&[0x12,0x34,0xab][..]}else{&[0,0,0][..]});
            }
            assert!(probe.frame(true).unwrap().iter().all(|v|*v==0));
        }}
        assert!(Probe::default().moved(-1).is_none());assert!(Probe::default().moved(0).is_none());
        assert!(Probe{index:479,group:true,..Default::default()}.moved(1).is_none());
        assert!(Probe{index:480,..Default::default()}.frame(false).is_none());
    }
    #[test]
    fn pixel_diagnostic_remote_observation_is_ordered_and_never_actuates() {
        let mut state=State::default();let current=observation(3);
        assert!(state.observe(&current));assert_eq!(state.probe().first(),474);
        assert!(state.pending.is_none() && state.armed.is_none());
        state.set_probe(Probe::default());
        assert!(!state.observe(&current));assert!(!state.observe(&observation(2)));
        assert_eq!(state.probe(),Probe::default()); // Duplicate push cannot replace a draft.
        let mut off=observation(4);off.led_output["diagnostic"]["off"]=true.into();
        assert!(state.observe(&off));assert_eq!(state.probe(),Probe::default());
        assert!(state.observed.as_ref().unwrap().label().contains("all 480 pixels off"));
        let mut invalid=observation(5);invalid.led_output["diagnostic"]["index"]=479.into();
        assert!(!state.observe(&invalid));
        let mut inactive=observation(5);inactive.led_output["diagnostic"]["active"]=false.into();
        assert!(state.observe(&inactive));assert!(!state.observed.as_ref().unwrap().active);
        state.revoke();assert!(state.observed.is_none());assert_eq!(state.probe(),Probe::default());
    }
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
