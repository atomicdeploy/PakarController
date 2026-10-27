//! Physical group and immutable frame forwarding through the selected owner.
//! Reuses the bridge's revocable route, peer fencing and existing serial actor;
//! discovery never opens another USB handle or authorizes an alternate route.
use super::*;
use crate::{pixel_diagnostic::Probe, pixel_groups::{self, Layout}, ui::cell_pixels::Frame};

pub(super) fn handles(method: &str, path: &str) -> bool {
    matches!((method, path), ("GET" | "POST", pixel_groups::PATH) | ("GET", "/api/pixels/frame"))
}

fn peer(o: &Observation) -> Result<peer_http::Pin, String> {
    peer_http::Pin::from_identity(&o.status.board_id, &o.status.boot_id).ok_or("Peer unavailable".into())
}

fn current(shared: &Shared, o: &Observation) -> Result<(), String> {
    let next = route(shared)?;
    if !o.route.current() || peer(&next)? != peer(o)? {
        return Err("Selected peer retired; nothing replayed".into());
    }
    Ok(())
}

/// A single execution in the existing owner. Cancellation expires queued work,
/// but an outcome already accepted by the board is never replayed elsewhere.
fn exchange(shared: &Shared, o: &Observation, method: &str, path: &str,
    body: Value, headers: &BTreeMap<String,String>, lifetime: Duration) -> Result<Value,String> {
    current(shared,o)?;
    require_pin(o,headers)?;
    let forwarding=trace(headers.get("x-pakar-route").map(String::as_str),&shared.settings.instance_id)?;
    let lifetime=lifetime.min(Duration::from_secs(8));
    if lifetime.is_zero() {return Err("Request expired; nothing sent".into());}
    let result=match &o.route {
        Route::Usb {port,admission,sender} => {
            let transaction=crate::usb_api::Transaction::new(&o.status,method,path,body)?;
            let mut admission=admission.clone();
            admission.at=Instant::now()-(Duration::from_secs(10)-lifetime);
            let cancelled=Arc::new(AtomicBool::new(false));
            let (tx,rx)=mpsc::channel();
            sender.send(SerialCommand::Api(port.clone(),crate::usb_api::Work {
                admission, transaction, reply:tx, wait:None,
                revoked:vec![cancelled.clone(),shared.stop.clone()],
            })).map_err(|_|"USB owner closed")?;
            let started=Instant::now();
            let outcome=loop {
                if let Err(error)=current(shared,o) {break Err(error);}
                let left=lifetime.saturating_sub(started.elapsed());
                if left.is_zero() {break Err("USB API outcome unavailable; no replay".into());}
                match rx.recv_timeout(left.min(Duration::from_millis(50))) {
                    Ok(value)=>break value,
                    Err(mpsc::RecvTimeoutError::Timeout)=>{},
                    Err(_)=>break Err("USB owner ended without a receipt; no replay".into()),
                }
            };
            cancelled.store(true,Ordering::Release);
            outcome?
        },
        Route::Network {endpoint,stamp,gate} => {
            let _lease=gate.acquire(Transport::WiFi,stamp.epoch).ok_or("Network owner revoked")?;
            let client=peer(o)?.client()?;
            let url=format!("{}{}",endpoint.base.trim_end_matches('/'),path);
            let mut request=if method=="GET" {client.get(url)} else {client.post(url)};
            request=request.timeout(lifetime.min(Duration::from_secs(4)))
                .basic_auth(&endpoint.user,Some(&endpoint.password)).header("X-Pakar-Route",forwarding);
            request=if method=="GET" {request.query(&body)} else {request.json(&body)};
            current(shared,o)?;
            let response=request.send().map_err(|_|"Upstream outcome unavailable; no replay")?;
            if !response.status().is_success() {return Err(format!("Upstream rejected request ({})",response.status()));}
            if response.content_length().is_some_and(|n|n>16384) {return Err("Upstream pixel receipt exceeds limit".into());}
            let mut bytes=Vec::new();response.take(16385).read_to_end(&mut bytes).map_err(|_|"Incomplete upstream receipt")?;
            if bytes.len()>16384 {return Err("Upstream pixel receipt exceeds limit".into());}
            serde_json::from_slice(&bytes).map_err(|_|"Invalid upstream pixel receipt")?
        },
    };
    current(shared,o)?;
    if result.get("accepted")==Some(&Value::Bool(false)) || result.get("error").is_some() {
        return Err(format!("Upstream rejected request: {}",result["error"].as_str().unwrap_or("not accepted")));
    }
    Ok(result)
}

fn frame(value: &Value, pin: &peer_http::Pin, layout: &Layout) -> Result<Frame,String> {
    if !pin.matches_identity(value["board_id"].as_str().unwrap_or(""),value["boot_id"].as_str().unwrap_or("")) {
        return Err("Pixel frame belongs to a different board session".into());
    }
    let frame:Frame=serde_json::from_value(value["pixel_frame"].clone()).map_err(|_|"Pixel frame unavailable")?;
    if !frame.valid() || frame.group_layout_revision!=Some(layout.revision)
        || frame.group_counts!=layout.counts || frame.group_starts!=layout.starts {
        return Err("Pixel frame layout changed or publication is incomplete".into());
    }
    Ok(frame)
}

fn configuration(request: &Request) -> Result<Value,String> {
    if request.headers.get("content-type").is_some_and(|v|v.starts_with("application/json")) {
        return serde_json::from_slice::<Value>(&request.body).map_err(|_|"Invalid group configuration JSON".into());
    }
    let body=std::str::from_utf8(&request.body).map_err(|_|"Invalid group configuration form")?;
    let url=reqwest::Url::parse(&format!("http://bridge/?{body}")).map_err(|_|"Invalid group configuration form")?;
    let pairs:Vec<_>=url.query_pairs().collect();
    if pairs.len()!=1 || pairs[0].0!="configuration" {return Err("Expected one configuration form field".into());}
    serde_json::from_str(&pairs[0].1).map_err(|_|"Invalid group configuration JSON".into())
}

fn draft(value:&Value, observed:&Layout)->Result<Layout,String> {
    if !value.is_object() || value["revision"].as_u64()!=Some(u64::from(observed.revision))
        || value.get("default_count").is_some_and(|v|v.as_u64()!=Some(u64::from(observed.default_count))) {
        return Err("Physical group revision/default changed; nothing sent".into());
    }
    let mut draft=observed.clone();
    draft.overrides=serde_json::from_value(value["overrides"].clone()).map_err(|_|"Complete physical group overrides required")?;
    draft.request()?;
    Ok(draft)
}

pub(super) fn handle(shared:&Shared,o:&Observation,request:&Request)->Result<Value,String> {
    if !pixel_groups::advertised(&o.status) {return Err("Physical group layout unavailable".into());}
    require_pin(o,&request.headers)?;
    let layout=pixel_groups::for_status(&o.status).ok_or("Current physical group layout unavailable")?;
    let pin=peer(o)?;
    let path=request.path.split('?').next().unwrap_or("");
    let query=reqwest::Url::parse(&format!("http://bridge{}",request.path)).map_err(|_|"Invalid pixel query")?;
    let values:Vec<_>=query.query_pairs().collect();
    if path=="/api/pixels/frame" {
        if values.len()>1 || values.iter().any(|(key,_)|key!="group_layout_revision") {return Err("Invalid pixel frame query".into());}
        let revision=values.first().map(|(_,v)|v.parse::<u32>()).transpose().map_err(|_|"Invalid pixel layout revision")?.unwrap_or(layout.revision);
        if revision!=layout.revision {return Err("Physical group layout changed; read configuration first".into());}
        let result=exchange(shared,o,"GET",path,json!({"group_layout_revision":revision}),&request.headers,Duration::from_secs(8))?;
        frame(&result,&pin,&layout)?;return Ok(result);
    }
    if !values.is_empty() {return Err("Unexpected physical group query".into());}
    let body=if request.method=="POST" {
        if o.status.programming_latch {return Err("Board programming active; nothing sent".into());}
        let draft=draft(&configuration(request)?,&layout)?;
        let result=exchange(shared,o,"POST",path,draft.request()?,&request.headers,Duration::from_secs(8))?;
        let actual=Layout::receipt(&result,&pin)?;
        if !draft.verifies(&actual) {return Err("Group write receipt differs; no replay".into());}
        // Read-only verification. A concurrent edit is reported, never overwritten.
        let read=exchange(shared,o,"GET",path,json!({}),&request.headers,Duration::from_secs(8))?;
        let actual=Layout::receipt(&read,&pin)?;
        if !draft.verifies(&actual) {return Err("Group layout changed after write; no replay".into());}
        read
    } else {
        let result=exchange(shared,o,"GET",path,json!({}),&request.headers,Duration::from_secs(8))?;
        Layout::receipt(&result,&pin)?;result
    };
    // Only group metadata changes here; a read must not renew unrelated status.
    let mut guard=shared.observation.lock().map_err(|_|"Bridge owner unavailable")?;
    if let Some(latest)=guard.as_mut().filter(|v|v.route.current() && peer(v).ok().as_ref()==Some(&pin)) {
        latest.status.pixel_groups=body.clone();
        if latest.mapping.is_object() {latest.mapping["pixel_groups"]=body.clone();}
        latest.status.pixel_frame=None;
    }
    Ok(body)
}

pub(super) fn diagnostic_form(request:&Value)->Vec<(String,String)> {
    ["index","count","rgb","off","group_layout_revision"].iter().filter_map(|key|
        request.get(*key).map(|v|((*key).to_owned(),v.as_str().map(str::to_owned).unwrap_or_else(||v.to_string())))).collect()
}

pub(super) fn diagnostic_matches(request:&Value,status:&DeviceStatus)->Result<bool,String> {
    let (probe,off)=Probe::from_form(status,&diagnostic_form(request)).ok_or("Diagnostic group layout changed")?;
    let output=&status.led_output["diagnostic"];
    let enabled=request["enabled"].as_bool().unwrap_or(true);
    if output["active"]!=enabled || output["index"].as_u64()!=Some(u64::from(probe.first()))
        || output["count"].as_u64()!=Some(probe.count().into()) || output["rgb"].as_u64()!=Some(probe.rgb.into())
        || output["off"]!=off || probe.group_layout_revision.is_some_and(|revision|output["group_layout_revision"].as_u64()!=Some(revision.into())) {
        return Ok(false);
    }
    for key in ["effect","composite","period_ms","duty_percent","min_brightness","max_brightness","enabled"] {
        if let Some(expected)=request.get(key) && output[key]!=*expected {return Ok(false);}
    }
    let Some(frame)=status.pixel_frame.as_ref().filter(|f|f.valid()) else {return Ok(false);};
    if let Some(revision)=probe.group_layout_revision {
        let layout=pixel_groups::for_status(status).ok_or("Physical group layout unavailable")?;
        if frame.group_layout_revision!=Some(revision) || frame.group_counts!=layout.counts || frame.group_starts!=layout.starts {return Ok(false);}
    }
    if !enabled || request["composite"]=="overlay" || request["effect"].as_str().is_some_and(|v|v!="solid") {
        return Ok(true); // Animated/overlay frames are not a static RGB assertion.
    }
    Ok(probe.matches(status,off))
}

pub(super) fn diagnostic(shared:&Shared,o:&Observation,request:&Value,headers:&BTreeMap<String,String>,lifetime:Duration)->Result<Value,String> {
    if o.status.programming_latch || !pixel_groups::advertised(&o.status) {return Err("Pixel group diagnostics unavailable".into());}
    Probe::from_form(&o.status,&diagnostic_form(request)).ok_or("Invalid or stale physical pixel group; nothing sent")?;
    let started=Instant::now();
    let mut result=exchange(shared,o,"POST","/api/command",request.clone(),headers,lifetime)?;
    let pin=peer(o)?;
    if result["accepted"]!=true || result["request_id"]!=request["request_id"]
        || !pin.matches_identity(result["board_id"].as_str().unwrap_or(""),result["boot_id"].as_str().unwrap_or("")) {
        return Err("Foreign diagnostic receipt; no replay".into());
    }
    let fragment=&result["status"];
    if !pin.matches_identity(fragment["board_id"].as_str().unwrap_or(""),fragment["boot_id"].as_str().unwrap_or("")) {
        return Err("Diagnostic readback missing or peer changed; no replay".into());
    }
    let mut merged=if result["status_partial"]==true {serde_json::to_value(&o.status).map_err(|e|e.to_string())?} else {json!({})};
    for (key,value) in fragment.as_object().ok_or("Invalid diagnostic readback")? {merged[key]=value.clone();}
    let published=fragment["pixel_frame"]["revision"].as_u64().ok_or("Diagnostic frame receipt unavailable; no replay")?;
    if fragment["pixel_frame"]["pending"]!=false {return Err("Diagnostic publication pending; no replay".into());}
    merged["pixel_frame"]=Value::Null; // Never inherit old output from before mutation.
    let mut status:DeviceStatus=serde_json::from_value(merged).map_err(|_|"Invalid diagnostic state")?;
    let layout=pixel_groups::for_status(&status).ok_or("Diagnostic layout missing")?;
    let value=exchange(shared,o,"GET","/api/pixels/frame",json!({"group_layout_revision":layout.revision}),headers,lifetime.saturating_sub(started.elapsed()))?;
    let frame=frame(&value,&pin,&layout)?;
    if u64::from(frame.revision)!=published {return Err("Diagnostic frame advanced before readback; no replay".into());}
    status.pixel_frame=Some(frame);
    if !diagnostic_matches(request,&status)? {return Err("Diagnostic outcome differs from requested output; no replay".into());}
    let next=Observation{status,mapping:o.mapping.clone(),route:o.route.clone(),at:o.at};
    result["status"]=status_value(shared,&next);
    result.as_object_mut().unwrap().remove("status_partial");
    *shared.observation.lock().unwrap()=Some(next);
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn layout()->Layout {
        let mut v=Layout::uniform(480,6).unwrap();v.revision=7;v.set_override(1,5);
        v.counts[0]=5;v.used_pixels=479;for start in &mut v.starts[1..] {*start-=1;}v
    }
    fn status()->DeviceStatus {
        serde_json::from_value(json!({"board_id":"fixture","boot_id":"a".repeat(32),"pixels":480,"http_peer_fencing":true,
            "capabilities":{"board_kind":"table","role":"table-selection","acknowledged_features":["pixel-group-layout","pixel-diagnostic"]},
            "pixel_groups":layout()})).unwrap()
    }
    #[test]
    fn pixel_groups_bridge_validates_full_replace_and_immutable_peer_bound_frame() {
        let layout=layout();let pin=peer_http::Pin::from_identity("fixture",&"a".repeat(32)).unwrap();
        assert!(draft(&json!({"revision":7,"overrides":[{"group":1,"count":4}]}),&layout).is_ok());
        assert!(draft(&json!({"revision":6,"overrides":[]}),&layout).is_err());
        assert!(draft(&json!({"revision":7,"overrides":[{"group":1,"count":4},{"group":1,"count":4}]}),&layout).is_err());
        let mut value=json!({"board_id":"fixture","boot_id":"a".repeat(32),"pixel_frame":{
            "rgb":"000000".repeat(480),"physical":(1..=80).collect::<Vec<_>>(),"revision":4,"brightness":255,"pending":false,
            "group_layout_revision":7,"group_counts":layout.counts,"group_starts":layout.starts}});
        assert!(frame(&value,&pin,&layout).is_ok());
        value["pixel_frame"]["group_layout_revision"]=8.into();assert!(frame(&value,&pin,&layout).is_err());
        value["pixel_frame"]["group_layout_revision"]=7.into();value["boot_id"]="b".repeat(32).into();assert!(frame(&value,&pin,&layout).is_err());
    }
    #[test]
    fn pixel_groups_bridge_diagnostic_readback_uses_actual_span_and_revision() {
        let mut status=status();let probe=Probe::for_status(&status,5,true,0x00ff00).unwrap();
        let request=json!({"index":5,"count":6,"rgb":65280,"off":false,"group_layout_revision":7});
        status.pixel_frame=Some(Frame{rgb:probe.frame(false).unwrap().iter().map(|v|format!("{v:02x}")).collect(),
            physical:(1..=80).collect(),revision:9,brightness:255,pending:false,group_layout_revision:Some(7),
            group_counts:layout().counts,group_starts:layout().starts});
        status.led_output=json!({"diagnostic":{"active":true,"index":5,"count":6,"rgb":65280,"off":false,"group_layout_revision":7}});
        assert!(diagnostic_matches(&request,&status).unwrap());
        let mut stale=request.clone();stale["group_layout_revision"]=6.into();assert!(diagnostic_matches(&stale,&status).is_err());
        status.pixel_frame.as_mut().unwrap().group_starts[1]=6;assert!(!diagnostic_matches(&request,&status).unwrap());
    }
}
