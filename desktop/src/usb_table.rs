//! Frozen USB table reads reuse the existing identity chunk shape, not its
//! mutable identity revision. PCController Session's matched ACK/admission and
//! no-replay rules apply to one operation on the selected USB owner.
use crate::{CellMapping, CellMappingResponse, DeviceStatus, peer_http};
use std::time::{Duration, Instant};
pub const READ: u8 = 0x5d;
pub const RESPONSE: u8 = 0xb6;
pub const COMMAND: u8 = 0x5e;
// Match the native current-peer authority window; capture time, not the final
// chunk's arrival, owns freshness. Slow captures expire rather than masquerade
// as new state. Windows first-byte COM timeout is not a per-chunk sleep.
pub const MAX_SNAPSHOT_AGE: Duration = Duration::from_secs(8);
#[derive(Default)]
pub struct Reader {
    pub nonce: u8,
    token: Option<u32>,
    total: usize,
    bytes: Vec<u8>,
    started: Option<Instant>,
}
pub struct Snapshot { pub status: DeviceStatus, pub mappings: Vec<CellMapping>, pub observed_at: Instant }
impl Reader {
    pub fn new(nonce:u8)->Self {Self{nonce,started:Some(Instant::now()),..Default::default()}}
    pub fn request(&self) -> Vec<u8> {
        let mut out=vec![self.nonce];out.extend((self.bytes.len() as u16).to_le_bytes());
        out.extend(self.token.unwrap_or(0).to_le_bytes());out
    }
    pub fn accept(&mut self,p:&[u8])->Result<Option<Snapshot>,String>{
        if self.started.is_some_and(|at|at.elapsed()>=MAX_SNAPSHOT_AGE){return Err("USB snapshot capture expired".into());}
        if !(10..=48).contains(&p.len()) || p[0]!=self.nonce {return Err("Invalid/stale USB table chunk".into());}
        let token=u32::from_le_bytes(p[1..5].try_into().unwrap());
        let total=u16::from_le_bytes(p[5..7].try_into().unwrap()) as usize;
        let offset=u16::from_le_bytes(p[7..9].try_into().unwrap()) as usize;
        if token==0 || total==0 || total>16384 || offset!=self.bytes.len() || offset+p.len()-9>total ||
            self.token.is_some_and(|old|old!=token || self.total!=total){return Err("Mixed/out-of-order USB table capture".into());}
        self.token=Some(token);self.total=total;self.bytes.extend_from_slice(&p[9..]);
        if self.bytes.len()!=total{return Ok(None);}
        let value:serde_json::Value=serde_json::from_slice(&self.bytes).map_err(|_|"Malformed USB table snapshot")?;
        let mut status:DeviceStatus=serde_json::from_value(value["status"].clone()).map_err(|_|"Invalid USB table status")?;
        if peer_http::Pin::from_status(&status).is_none() || status.pixels!=480 || status.cells.len()!=80 ||
            !status.cells.iter().all(|v|matches!(v.as_str(),"white"|"red"|"off")) ||
            !status.capabilities.as_ref().is_some_and(|c|c.table()) {return Err("USB table identity/capabilities/state incomplete".into());}
        // Optional on older firmware. The frame is actual last-published RGB,
        // captured once with this token; never derive six pixels from cell state.
        status.pixel_preview_requested=true;
        status.pixel_frame=match value.get("pixels").filter(|v|!v.is_null()) {
            Some(value)=>{
                let frame:crate::ui::cell_pixels::Frame=serde_json::from_value(value.clone()).map_err(|_|"Invalid USB pixel snapshot")?;
                if !frame.valid(){return Err("Invalid USB pixel snapshot".into());}
                Some(frame)
            }
            None=>None,
        };
        let mapping:CellMappingResponse=serde_json::from_value(value).map_err(|_|"Invalid USB table mapping")?;
        Ok(Some(Snapshot{status,mappings:mapping.decode()?,observed_at:self.started.unwrap_or_else(Instant::now)}))
    }
}
pub fn table_command(op:u8)->bool {matches!(op,0x4b|0x4c|0x4d|0x51|0x52|0x5c|0x5f)}
pub fn fenced(status:&DeviceStatus,op:u8,payload:&[u8])->Result<Vec<u8>,String>{
    if !table_command(op) || payload.len()>31 || peer_http::Pin::from_status(status).is_none(){return Err("USB table command lacks a verified peer".into());}
    if op==crate::cell_batch::OPCODE && (!crate::cell_batch::Request::supported(status) || crate::cell_batch::Request::from_payload(payload).is_none()){
        return Err("Atomic USB cell batch unavailable/invalid; nothing sent, no per-cell fallback".into());
    }
    let mut out=Vec::with_capacity(17+payload.len());
    for pair in status.boot_id.as_bytes().chunks_exact(2){
        let nibble=|v:u8|if v.is_ascii_digit(){v-b'0'}else{v-b'a'+10};
        out.push(nibble(pair[0])*16+nibble(pair[1]));
    }
    out.push(op);out.extend_from_slice(payload);Ok(out)
}
#[derive(Clone)]
pub struct Admission {
    pub stamp:crate::connection::Stamp,
    pub session:u64,
    pub peer:Option<peer_http::Pin>,
    pub at:Instant,
    pub batch:Option<crate::cell_batch::Correlation>,
}
impl Admission {
    pub fn current(&self,session:u64,status:Option<&DeviceStatus>)->bool {
        self.session!=0 && self.session==session && self.stamp.current() &&
            self.at.elapsed()<Duration::from_secs(10) && self.peer==status.and_then(peer_http::Pin::from_status) &&
            self.batch.as_ref().is_none_or(|batch|Some(&batch.peer)==self.peer.as_ref() && batch.route==crate::Transport::Serial)
    }
}
pub struct Intent {pub opcode:u8,pub payload:Vec<u8>,pub admission:Admission}
impl Intent {
    pub fn current(&self,session:u64,status:Option<&DeviceStatus>)->bool {
        self.admission.peer.is_some() && self.admission.current(session,status) &&
            (self.opcode!=crate::cell_batch::OPCODE || self.admission.batch.as_ref()
                .is_some_and(|batch|batch.request.payload()==self.payload))
    }
    pub fn verify(&self,s:&DeviceStatus)->bool {
        match (self.opcode,self.payload.as_slice()) {
            (0x5f,payload)=>self.admission.batch.as_ref().is_some_and(|batch|
                batch.request.payload()==payload && crate::peer_http::Pin::from_status(s).as_ref()==Some(&batch.peer) && batch.request.verify(s)),
            (0x4b,[mode]) => ["unknown","cell","first-wins","snake"].get(*mode as usize).is_some_and(|v|s.mode==*v),
            (0x4c,[id,state,..]) => *id>=1 && *id<=80 && ["white","red","off"].get(*state as usize).is_some_and(|v|s.cells.get(*id as usize-1).is_some_and(|actual|actual.as_str()==*v)),
            (0x4d,[]) => s.cells.iter().all(|v|v=="white") && s.winner==0,
            (0x51,[brightness]) => s.brightness==*brightness,
            (0x52,[pattern]) => ["none","rainbow","fire"].get(*pattern as usize).is_some_and(|v|s.effect==*v),
            (0x5c,[enabled]) if *enabled<=1 => s.led_output["enabled"].as_bool()==Some(*enabled!=0),
            (0x5c,[2,mode]) if *mode<=1 => s.led_output["mode"].as_str()==Some(if *mode==0{"static"}else{"effects"}),
            _ => false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn fixture()->serde_json::Value {
        serde_json::json!({"status":{"hostname":"ledboard","board_id":"esp32s3-fixture","boot_id":"a".repeat(32),
            "http_peer_fencing":true,"pixels":480,"mode":"cell","cells":vec!["white";80],
            "led_output":{"enabled":true,"mode":"static"},
            "capabilities":{"board_kind":"table","acknowledged_features":["table","leds"],
                "table":{"columns":10,"rows":8,"pixels_per_cell":6}}},
            "physical":(1..=80).collect::<Vec<_>>(),"names":vec!["cell";80]})
    }
    fn chunk(bytes:&[u8],offset:usize,nonce:u8,token:u32)->Vec<u8>{
        let mut p=vec![nonce];p.extend(token.to_le_bytes());p.extend((bytes.len() as u16).to_le_bytes());
        p.extend((offset as u16).to_le_bytes());p.extend_from_slice(&bytes[offset..(offset+39).min(bytes.len())]);p
    }
    fn read(value:&serde_json::Value)->Snapshot{
        let bytes=serde_json::to_vec(value).unwrap();let mut reader=Reader::new(9);let mut result=None;
        for offset in (0..bytes.len()).step_by(39){result=reader.accept(&chunk(&bytes,offset,9,17)).unwrap();}
        result.unwrap()
    }
    #[test]
    fn immutable_usb_snapshot_requires_complete_identity_states_and_unique_mapping(){
        let mut value=fixture();let snapshot=read(&value);
        assert_eq!(snapshot.status.cells.len(),80);assert_eq!(snapshot.mappings.len(),80);
        assert!(snapshot.status.pixel_frame.is_none());
        for field in ["boot_id","board_id"] {
            let mut invalid=value.clone();invalid["status"][field]=serde_json::json!("");
            let bytes=serde_json::to_vec(&invalid).unwrap();let mut reader=Reader::new(9);let mut failed=false;
            for offset in (0..bytes.len()).step_by(39){if reader.accept(&chunk(&bytes,offset,9,17)).is_err(){failed=true;break;}}
            assert!(failed);
        }
        value["physical"]=serde_json::json!(vec![1;80]);
        let bytes=serde_json::to_vec(&value).unwrap();let mut reader=Reader::new(9);
        assert!((0..bytes.len()).step_by(39).any(|offset|reader.accept(&chunk(&bytes,offset,9,17)).is_err()));
    }
    #[test]
    fn frozen_usb_rgb_reuses_published_frame_parser_and_capture_time(){
        let mut value=fixture();
        value["pixels"]=serde_json::json!({"rgb":"112233".repeat(480),
            "physical":(1..=80).rev().collect::<Vec<_>>(),"revision":7,"brightness":203,"pending":true});
        // A larger coherent capture is supported without another transport.
        value["optional_metadata"]=serde_json::Value::String("x".repeat(8192));
        let bytes=serde_json::to_vec(&value).unwrap();assert!(bytes.len()>8192 && bytes.len()<16384);
        let mut reader=Reader::new(9);let captured_at=reader.started.unwrap();let mut snapshot=None;
        for offset in (0..bytes.len()).step_by(39){snapshot=reader.accept(&chunk(&bytes,offset,9,17)).unwrap();}
        let snapshot=snapshot.unwrap();assert_eq!(snapshot.observed_at,captured_at);
        let frame=snapshot.status.pixel_frame.unwrap();
        assert!(snapshot.status.pixel_preview_requested);assert_eq!(frame.revision,7);assert_eq!(frame.brightness,203);
        let (first,colors)=frame.cell(1).unwrap();assert_eq!(first,474);
        assert!(colors.iter().all(|color|*color==eframe::egui::Color32::from_rgb(17,34,51)));
        value["pixels"]["rgb"]="zz0000".repeat(480).into();
        let bytes=serde_json::to_vec(&value).unwrap();let mut reader=Reader::new(9);
        assert!((0..bytes.len()).step_by(39).any(|offset|reader.accept(&chunk(&bytes,offset,9,17)).is_err()));
        let mut reader=Reader::new(9);let mut oversized=chunk(&bytes,0,9,17);
        oversized[5..7].copy_from_slice(&16385_u16.to_le_bytes());assert!(reader.accept(&oversized).is_err());
    }
    #[test]
    fn chunks_reject_wrong_nonce_token_offset_and_total_without_promotion(){
        let bytes=serde_json::to_vec(&fixture()).unwrap();let mut reader=Reader::new(9);
        assert_eq!(reader.request(),vec![9,0,0,0,0,0,0]);
        assert!(reader.accept(&chunk(&bytes,0,8,17)).is_err());
        assert!(reader.accept(&chunk(&bytes,0,9,0)).is_err());
        assert!(reader.accept(&chunk(&bytes,0,9,17)).unwrap().is_none());
        assert!(reader.accept(&chunk(&bytes,39,9,18)).is_err());
        assert!(reader.accept(&chunk(&bytes,0,9,17)).is_err());
        let mut wrong=chunk(&bytes,39,9,17);wrong[5]^=1;assert!(reader.accept(&wrong).is_err());
    }
    #[test]
    fn table_write_is_exact_boot_fenced_and_post_ack_values_are_not_assumed(){
        let snapshot=read(&fixture());let s=snapshot.status;
        let packet=fenced(&s,0x4c,&[1,1,0]).unwrap();
        assert_eq!(&packet[..16],&[0xaa;16]);assert_eq!(&packet[16..],&[0x4c,1,1,0]);
        assert!(fenced(&s,0x55,&[0]).is_err());assert!(fenced(&s,0x4c,&[0;32]).is_err());
        let gate=crate::connection::Gate::default();let epoch=gate.select(Some(crate::connection::Method::Serial));
        let admission=Admission{stamp:gate.stamp(crate::connection::Method::Serial,epoch),session:1,peer:peer_http::Pin::from_status(&s),at:Instant::now(),batch:None};
        let intent=Intent{opcode:0x4c,payload:vec![1,1,0],admission};
        assert!(intent.current(1,Some(&s)));assert!(!intent.current(2,Some(&s)));assert!(!intent.verify(&s));
        let mut applied=s.clone();applied.cells[0]="red".into();assert!(intent.verify(&applied));
        applied.boot_id="b".repeat(32);assert!(!intent.current(1,Some(&applied)));
        gate.select(Some(crate::connection::Method::Serial));assert!(!intent.current(1,Some(&s)));
    }
    #[test]
    fn atomic_batch_uses_one_boot_fenced_bitmap_and_verifies_every_selected_state(){
        let mut status=read(&fixture()).status;
        let request=crate::cell_batch::Request::new(&[1,80],"red","fade").unwrap();
        assert!(fenced(&status,0x5f,&request.payload()).is_err());
        status.capabilities.as_mut().unwrap().acknowledged_features.push("cell-batch".into());
        let packet=fenced(&status,0x5f,&request.payload()).unwrap();
        assert_eq!(packet.len(),29);assert_eq!(packet[16],0x5f);
        let gate=crate::connection::Gate::default();let epoch=gate.select(Some(crate::connection::Method::Serial));
        let admission=Admission{stamp:gate.stamp(crate::connection::Method::Serial,epoch),session:1,
            peer:peer_http::Pin::from_status(&status),at:Instant::now(),
            batch:Some(crate::cell_batch::Correlation::new(request.clone(),peer_http::Pin::from_status(&status).unwrap(),crate::Transport::Serial).unwrap())};
        let intent=Intent{opcode:0x5f,payload:request.payload(),admission};
        assert!(intent.current(1,Some(&status)));
        let mut changed=Intent{opcode:0x5f,payload:request.payload(),admission:intent.admission.clone()};
        changed.payload[2]^=2;assert!(!changed.current(1,Some(&status)));
        assert!(!intent.verify(&status));status.cells[0]="red".into();assert!(!intent.verify(&status));
        status.cells[79]="red".into();assert!(intent.verify(&status));
        status.boot_id="b".repeat(32);assert!(!intent.current(1,Some(&status)));
    }
}
