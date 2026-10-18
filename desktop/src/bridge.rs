//! Selected-owner API sharing. PCController's bridge correlation, no-replay,
//! public discovery metadata and lifecycle rules are adapted, not its media RPC.
use crate::{DeviceStatus, Endpoint, SerialCommand, Transport, connection, peer_http, usb_table};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    collections::BTreeMap,
    io::{Read, Write},
    net::{IpAddr, SocketAddr, TcpListener, TcpStream},
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, AtomicUsize, Ordering},
        mpsc,
    },
    thread,
    time::{Duration, Instant},
};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub enabled: bool,
    pub listen: String,
    pub port: u16,
    pub hostname: String,
    pub instance_id: String,
    pub mdns: bool,
    pub ssdp: bool,
    pub mndp: bool,
}
impl Default for Settings {
    fn default() -> Self {
        Self {
            enabled: false,
            listen: "0.0.0.0".into(),
            port: 8789,
            hostname: std::env::var("COMPUTERNAME")
                .or_else(|_| std::env::var("HOSTNAME"))
                .unwrap_or_else(|_| "pakarcontroller".into())
                .to_ascii_lowercase(),
            instance_id: uuid::Uuid::new_v4().to_string(),
            mdns: true,
            ssdp: true,
            mndp: true,
        }
    }
}
impl Settings {
    pub fn validate(&self) -> Result<SocketAddr, String> {
        let ip: IpAddr = self
            .listen
            .parse()
            .map_err(|_| "Choose a numeric bridge listen address")?;
        if ip.is_multicast()
            || self.port == 0
            || crate::identity::normalize_name(&self.hostname).as_deref()
                != Ok(self.hostname.as_str())
            || uuid::Uuid::parse_str(&self.instance_id).is_err()
        {
            return Err("Invalid bridge hostname, instance identity or port".into());
        }
        Ok(SocketAddr::new(ip, self.port))
    }
}
#[derive(Clone)]
pub enum Route {
    Usb {
        port: String,
        admission: usb_table::Admission,
        sender: mpsc::Sender<SerialCommand>,
    },
    Network {
        endpoint: Endpoint,
        stamp: connection::Stamp,
        gate: connection::Gate,
    },
}
impl Route {
    fn current(&self) -> bool {
        match self {
            Self::Usb { admission, .. } => admission.stamp.current(),
            Self::Network { stamp, .. } => stamp.current(),
        }
    }
}
pub struct Observation {
    pub status: DeviceStatus,
    pub mapping: Value,
    pub route: Route,
    pub at: Instant,
}
struct Shared {
    settings: Settings,
    observation: Mutex<Option<Observation>>,
    stop: Arc<AtomicBool>,
    clients: AtomicUsize,
    receipts: Mutex<BTreeMap<String, (Instant, Value, Option<Value>)>>,
    last_input: Mutex<Value>,
}
pub struct Runtime {
    settings: Option<Settings>,
    shared: Option<Arc<Shared>>,
    discovery: Option<crate::platform::discovery::Advertiser>,
    pub error: String,
}
impl Default for Runtime {
    fn default() -> Self {
        Self {
            settings: None,
            shared: None,
            discovery: None,
            error: String::new(),
        }
    }
}
impl Runtime {
    pub fn reconcile(&mut self, settings: &Settings, fixture: bool) {
        if self.settings.as_ref() == Some(settings) {
            return;
        }
        self.stop();
        self.settings = Some(settings.clone());
        self.error.clear();
        if fixture || !settings.enabled {
            return;
        }
        match settings
            .validate()
            .and_then(|address| TcpListener::bind(address).map_err(|e| e.to_string()))
        {
            Ok(listener) => {
                let _ = listener.set_nonblocking(true);
                let shared = Arc::new(Shared {
                    settings: settings.clone(),
                    observation: Mutex::new(None),
                    stop: Arc::new(AtomicBool::new(false)),
                    clients: AtomicUsize::new(0),
                    receipts: Mutex::new(BTreeMap::new()),
                    last_input: Mutex::new(Value::Null),
                });
                let owner = shared.clone();
                thread::spawn(move || {
                    while !owner.stop.load(Ordering::Acquire) {
                        match listener.accept() {
                            Ok((socket, _)) => {
                                if owner.clients.fetch_add(1, Ordering::AcqRel) >= 16 {
                                    owner.clients.fetch_sub(1, Ordering::AcqRel);
                                    continue;
                                }
                                let shared = owner.clone();
                                thread::spawn(move || {
                                    let _ = serve(socket, &shared);
                                    shared.clients.fetch_sub(1, Ordering::AcqRel);
                                });
                            }
                            Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                                thread::sleep(Duration::from_millis(20))
                            }
                            Err(_) => break,
                        }
                    }
                });
                self.discovery = Some(crate::platform::discovery::Advertiser::start(settings));
                self.shared = Some(shared);
            }
            Err(error) => self.error = error,
        }
    }
    pub fn observe(&self, observation: Option<Observation>) {
        if let Some(shared) = &self.shared {
            let value = observation
                .as_ref()
                .map(|o| json!({"status":o.status,"mapping":o.mapping}))
                .unwrap_or(Value::Null);
            let mut previous = shared.last_input.lock().unwrap();
            if *previous != value || observation.is_none() {
                *previous = value;
                *shared.observation.lock().unwrap() = observation;
            }
        }
    }
    pub fn snapshot(&self) -> Value {
        json!({"enabled":self.settings.as_ref().is_some_and(|s|s.enabled),"listening":self.shared.is_some(),
        "error":self.error,"discovery":self.discovery.as_ref().map(|d|d.snapshot())})
    }
    pub fn stop(&mut self) {
        if let Some(shared) = self.shared.take() {
            shared.stop.store(true, Ordering::Release);
        }
        self.discovery = None;
    }
}
impl Drop for Runtime {
    fn drop(&mut self) {
        self.stop();
    }
}

fn route(shared: &Shared) -> Result<Observation, String> {
    if shared.stop.load(Ordering::Acquire) {
        return Err("Bridge stopped".into());
    }
    let guard = shared
        .observation
        .lock()
        .map_err(|_| "Bridge owner unavailable")?;
    let o = guard
        .as_ref()
        .filter(|o| o.at.elapsed() < Duration::from_secs(8) && o.route.current())
        .ok_or("Selected board unavailable")?;
    if let Some(path) = o.status.bridge.as_ref().and_then(|v| v["path"].as_array()) {
        if path.is_empty()
            || path.len() >= 4
            || path.iter().any(|v| {
                v.as_str() == Some(&shared.settings.instance_id)
                    || !v.as_str().is_some_and(|s| uuid::Uuid::parse_str(s).is_ok())
            })
            || path
                .iter()
                .enumerate()
                .any(|(i, id)| path[..i].contains(id))
        {
            return Err("Cyclic bridge observation rejected".into());
        }
    }
    Ok(Observation {
        status: o.status.clone(),
        mapping: o.mapping.clone(),
        route: o.route.clone(),
        at: o.at,
    })
}
fn status_value(shared: &Shared, o: &Observation) -> Value {
    let mut status = o.status.clone();
    if let Some(c) = &mut status.capabilities {
        // These require opaque/binary transfer, which this bounded command
        // relay deliberately does not yet implement. Do not promise them.
        c.acknowledged_features
            .retain(|s| !matches!(s.as_str(), "ota" | "isp" | "streaming"));
    }
    if matches!(o.route, Route::Usb { .. }) {
        // Advertise only operations implemented by this route, not every
        // capability reachable through a separate, unselected Wi-Fi connection.
        if let Some(c) = &mut status.capabilities {
            c.acknowledged_features
                .retain(|s| matches!(s.as_str(), "table" | "leds" | "cell-batch" | "command-ack"));
            c.modes.retain(|m| m.id == "cell");
        }
    }
    let mut v = serde_json::to_value(status).unwrap_or(Value::Null);
    let mut path = o
        .status
        .bridge
        .as_ref()
        .and_then(|v| v["path"].as_array())
        .cloned()
        .unwrap_or_default();
    path.push(json!(shared.settings.instance_id));
    v["bridge"] = json!({"instance_id":shared.settings.instance_id,"hostname":shared.settings.hostname,"product":"PakarController","path":path,
        "transport":if matches!(o.route,Route::Usb{..}){"usb"}else{"network"},"max_hops":4});
    v["command_clock_ms"] = clock(o).map_or(Value::Null, |v| json!(v));
    v
}
pub fn matches_hostname(status: &DeviceStatus, expected: &str) -> bool {
    if status.hostname == expected {
        return true;
    }
    let Some(b) = &status.bridge else {
        return false;
    };
    let Some(path) = b["path"].as_array() else {
        return false;
    };
    b["product"] == "PakarController"
        && b["hostname"] == expected
        && !path.is_empty()
        && path.len() <= 4
        && path.last() == Some(&b["instance_id"])
        && path.iter().all(|id| {
            id.as_str()
                .is_some_and(|s| uuid::Uuid::parse_str(s).is_ok())
        })
        && path
            .iter()
            .enumerate()
            .all(|(i, id)| !path[..i].contains(id))
}
fn clock(o: &Observation) -> Result<u64, String> {
    o.status
        .command_clock_ms
        .and_then(|v| v.checked_add(o.at.elapsed().as_millis() as u64))
        .ok_or("Board command clock unavailable".into())
}
fn trace(value: Option<&str>, id: &str) -> Result<String, String> {
    let parts: Vec<_> = value
        .unwrap_or("")
        .split(',')
        .filter(|v| !v.is_empty())
        .collect();
    if parts.len() >= 4
        || parts
            .iter()
            .any(|v| uuid::Uuid::parse_str(v).is_err() || *v == id)
    {
        return Err("Bridge forwarding loop or hop limit".into());
    }
    Ok(parts
        .into_iter()
        .chain(Some(id))
        .collect::<Vec<_>>()
        .join(","))
}
fn require_pin(o: &Observation, headers: &BTreeMap<String, String>) -> Result<(), String> {
    if headers.get("x-pakar-board").map(String::as_str) != Some(o.status.board_id.as_str())
        || headers.get("x-pakar-boot").map(String::as_str) != Some(o.status.boot_id.as_str())
    {
        return Err("Board/boot identity required or changed".into());
    }
    Ok(())
}
pub struct Reply {
    sender: mpsc::Sender<Result<DeviceStatus, String>>,
    stop: Arc<AtomicBool>,
}
impl Reply {
    pub fn new(sender: mpsc::Sender<Result<DeviceStatus, String>>, stop: Arc<AtomicBool>) -> Self {
        Self { sender, stop }
    }
    pub fn current(&self) -> bool {
        !self.stop.load(Ordering::Acquire)
    }
    pub fn send(
        &self,
        result: Result<DeviceStatus, String>,
    ) -> Result<(), mpsc::SendError<Result<DeviceStatus, String>>> {
        self.sender.send(if self.current() {
            result
        } else {
            Err("Sharing stopped; outcome not replayed".into())
        })
    }
}
fn usb_request(
    path: &str,
    form: &[(String, String)],
    status: &DeviceStatus,
) -> Result<(u8, Vec<u8>, Option<crate::cell_batch::Correlation>), String> {
    if !status
        .capabilities
        .as_ref()
        .is_some_and(|c| c.allows(path, form))
    {
        return Err("Capability unavailable".into());
    }
    let get = |key: &str| {
        form.iter()
            .find(|(k, _)| k == key)
            .map(|(_, v)| v.as_str())
            .ok_or_else(|| format!("Missing {key}"))
    };
    Ok(match path {
        "/api/cell" => {
            let id = get("id")?.parse().map_err(|_| "Invalid cell")?;
            (
                0x4c,
                crate::ui::shell::led_cell_payload(
                    id,
                    get("state")?,
                    get("effect").unwrap_or("immediate"),
                )
                .ok_or("Invalid cell command")?,
                None,
            )
        }
        "/api/cells" => {
            let batch = crate::cell_batch::Request::new(
                &crate::ui::cell_search::parse(get("ids")?)?,
                get("state")?,
                get("effect").unwrap_or("immediate"),
            )?;
            let correlation = crate::cell_batch::Correlation::new(
                batch,
                peer_http::Pin::from_status(status).ok_or("Peer unavailable")?,
                Transport::Serial,
            )?;
            (
                crate::cell_batch::OPCODE,
                correlation.request.payload(),
                Some(correlation),
            )
        }
        "/api/brightness" => {
            let value: u8 = get("value")?.parse().map_err(|_| "Invalid brightness")?;
            if value == 0 {
                return Err("Invalid brightness".into());
            }
            (0x51, vec![value], None)
        }
        "/api/effect" => (
            0x52,
            vec![crate::ui::shell::led_effect_code(get("value")?).ok_or("Unsupported effect")?],
            None,
        ),
        "/api/mode" => (
            0x4b,
            vec![crate::ui::shell::led_mode_code(get("value")?).ok_or("Unsupported mode")?],
            None,
        ),
        "/api/led/output" => {
            let value = match form
                .iter()
                .find(|(k, _)| k == "enabled")
                .map(|(_, v)| v.as_str())
            {
                Some("true") => "on",
                Some("false") => "off",
                Some(_) => return Err("Invalid enabled value".into()),
                None => get("mode")?,
            };
            if form.len() != 1 {
                return Err("Choose enabled OR mode in one operation".into());
            }
            (
                0x5c,
                crate::ui::shell::led_output_request(value)
                    .ok_or("Invalid LED output")?
                    .1,
                None,
            )
        }
        _ => return Err("This API is not implemented on the selected USB route".into()),
    })
}
fn mutate(
    shared: &Shared,
    o: &Observation,
    path: &str,
    form: Vec<(String, String)>,
    headers: &BTreeMap<String, String>,
    body: &[u8],
    lifetime: Duration,
) -> Result<Value, String> {
    if shared.stop.load(Ordering::Acquire) || !o.route.current() {
        return Err("Sharing or selected owner retired; nothing sent".into());
    }
    require_pin(o, headers)?;
    if o.status.programming_latch
        || !o
            .status
            .capabilities
            .as_ref()
            .is_some_and(|c| c.allows(path, &form))
    {
        return Err("Capability unavailable or board programming active".into());
    }
    let forwarding = trace(
        headers.get("x-pakar-route").map(String::as_str),
        &shared.settings.instance_id,
    )?;
    match &o.route {
        Route::Usb {
            port,
            admission,
            sender,
        } => {
            let (op, payload, batch) = usb_request(path, &form, &o.status)?;
            let (tx, rx) = mpsc::channel();
            let mut admission = admission.clone();
            admission.at =
                Instant::now() - (Duration::from_secs(10) - lifetime.min(Duration::from_secs(8)));
            admission.batch = batch;
            sender
                .send(SerialCommand::Bridge(
                    port.clone(),
                    admission,
                    op,
                    payload,
                    Reply::new(tx, shared.stop.clone()),
                ))
                .map_err(|_| "USB owner closed")?;
            let status = rx
                .recv_timeout(Duration::from_secs(8))
                .map_err(|_| "USB outcome uncertain; not replayed")??;
            if shared.stop.load(Ordering::Acquire)
                || !o.route.current()
                || peer_http::Pin::from_status(&status) != peer_http::Pin::from_status(&o.status)
            {
                return Err("Peer retired after dispatch; no replay".into());
            }
            let next = Observation {
                status,
                mapping: o.mapping.clone(),
                route: o.route.clone(),
                at: o.at,
            };
            let result = json!({"accepted":true,"count":form.iter().find(|(k,_)|k=="ids").and_then(|(_,v)|crate::ui::cell_search::parse(v).ok()).map(|s|s.len()),"status":status_value(shared,&next)});
            *shared.observation.lock().unwrap() = Some(next);
            Ok(result)
        }
        Route::Network {
            endpoint,
            stamp,
            gate,
        } => {
            let _lease = gate
                .acquire(Transport::WiFi, stamp.epoch)
                .ok_or("Network owner revoked")?;
            let client = peer_http::Pin::from_status(&o.status)
                .ok_or("Peer unavailable")?
                .client()?;
            let mut request = client
                .post(format!("{}{}", endpoint.base.trim_end_matches('/'), path))
                .timeout(lifetime.min(Duration::from_secs(4)))
                .basic_auth(&endpoint.user, Some(&endpoint.password))
                .header("X-Pakar-Route", forwarding);
            request = if path == "/api/command" {
                request
                    .header("Content-Type", "application/json")
                    .body(body.to_vec())
            } else {
                request.form(&form)
            };
            if !stamp.current() || shared.stop.load(Ordering::Acquire) {
                return Err("Network owner revoked".into());
            }
            let response = request
                .send()
                .map_err(|e| format!("Network outcome uncertain; not replayed: {e}"))?;
            if !stamp.current() || shared.stop.load(Ordering::Acquire) {
                return Err("Network owner changed after dispatch; not replayed".into());
            }
            if !response.status().is_success() {
                return Err(format!("Upstream rejected command ({})", response.status()));
            }
            let mut result: Value = response
                .json()
                .map_err(|_| "Upstream result unavailable; not replayed".to_string())?;
            if result["accepted"] != true {
                return Err("Upstream did not acknowledge command".into());
            }
            let status = crate::blocking_status(&client, endpoint)?;
            if shared.stop.load(Ordering::Acquire)
                || !stamp.current()
                || peer_http::Pin::from_status(&status) != peer_http::Pin::from_status(&o.status)
            {
                return Err("Upstream peer changed after dispatch; no replay".into());
            }
            let next = Observation {
                status,
                mapping: o.mapping.clone(),
                route: o.route.clone(),
                at: Instant::now(),
            };
            result["status"] = status_value(shared, &next);
            *shared.observation.lock().unwrap() = Some(next);
            Ok(result)
        }
    }
}
fn command(shared: &Shared, request: Value, headers: &BTreeMap<String, String>) -> Value {
    let id = request["request_id"].as_str().unwrap_or("").to_string();
    let mut owns_receipt = false;
    let mut reply = json!({"type":"command-result","request_id":id,"board_id":request["board_id"],"boot_id":request["boot_id"],"accepted":false});
    let result = (|| -> Result<Value, String> {
        let o = route(shared)?;
        if id.is_empty()
            || id.len() > 128
            || !id
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || b == b'-' || b == b'_')
            || request["type"] != "command"
            || request["board_id"] != o.status.board_id
            || request["boot_id"] != o.status.boot_id
        {
            return Err("Invalid command identity".into());
        }
        let now = clock(&o)?;
        let deadline = request["expires_at_ms"]
            .as_u64()
            .ok_or("Command deadline required")?;
        {
            let mut receipts = shared.receipts.lock().unwrap();
            receipts.retain(|_, (at, _, _)| at.elapsed() < Duration::from_secs(60));
            if let Some((_, previous, value)) = receipts.get(&id) {
                return if previous == &request {
                    value
                        .clone()
                        .ok_or("Command already dispatched; outcome pending, not replayed".into())
                } else {
                    Err("Request ID reused for another intent".into())
                };
            }
            if deadline <= now || deadline > now + 8000 {
                return Err("Command deadline expired or invalid".into());
            }
            if receipts.len() >= 1024 {
                return Err("Command receipt budget full".into());
            }
            receipts.insert(id.clone(), (Instant::now(), request.clone(), None));
            owns_receipt = true;
        }
        let path = match request["command"].as_str() {
            Some("cell") => "/api/cell",
            Some("cells") => "/api/cells",
            Some("effect") => "/api/effect",
            Some("mode") => "/api/mode",
            Some("brightness") => "/api/brightness",
            Some("led-output") => "/api/led/output",
            _ => return Err("Unsupported command on bridge".into()),
        };
        let mut form = Vec::new();
        for key in ["id", "ids", "state", "effect", "value", "enabled", "mode"] {
            if let Some(v) = request.get(key) {
                form.push((
                    key.into(),
                    if let Some(s) = v.as_str() {
                        s.into()
                    } else {
                        v.to_string()
                    },
                ));
            }
        }
        let mut pin = headers.clone();
        pin.insert("x-pakar-board".into(), o.status.board_id.clone());
        pin.insert("x-pakar-boot".into(), o.status.boot_id.clone());
        let mut result = mutate(
            shared,
            &o,
            path,
            form,
            &pin,
            &[],
            Duration::from_millis(deadline - now),
        )?;
        result["type"] = json!("command-result");
        result["request_id"] = json!(id);
        result["board_id"] = json!(o.status.board_id);
        result["boot_id"] = json!(o.status.boot_id);
        Ok(result)
    })();
    match result {
        Ok(value) => reply = value,
        Err(error) => reply["error"] = json!(error),
    }
    if owns_receipt {
        if let Some(entry) = shared.receipts.lock().unwrap().get_mut(&id) {
            if entry.1 == request {
                entry.2 = Some(reply.clone());
            }
        }
    }
    reply
}
struct Request {
    method: String,
    path: String,
    headers: BTreeMap<String, String>,
    body: Vec<u8>,
}
fn read_request(socket: &mut TcpStream) -> Result<Request, String> {
    socket
        .set_read_timeout(Some(Duration::from_millis(250)))
        .map_err(|e| e.to_string())?;
    let deadline = Instant::now() + Duration::from_secs(4);
    let mut bytes = Vec::new();
    loop {
        let mut headers = [httparse::EMPTY_HEADER; 40];
        if Instant::now() >= deadline || bytes.len() > 16384 {
            return Err("HTTP header limit/deadline".into());
        }
        let mut parser = httparse::Request::new(&mut headers);
        if let httparse::Status::Complete(offset) =
            parser.parse(&bytes).map_err(|e| e.to_string())?
        {
            let mut map = BTreeMap::new();
            for header in parser.headers.iter() {
                let key = header.name.to_ascii_lowercase();
                if map
                    .insert(
                        key,
                        std::str::from_utf8(header.value)
                            .map_err(|_| "Invalid header")?
                            .to_owned(),
                    )
                    .is_some()
                {
                    return Err("Duplicate HTTP header".into());
                }
            }
            if map.contains_key("transfer-encoding") || map.contains_key("origin") {
                return Err("Chunked bodies and browser-origin commands unavailable".into());
            }
            let length = map
                .get("content-length")
                .map(|v| v.parse::<usize>())
                .transpose()
                .map_err(|_| "Invalid body length")?
                .unwrap_or(0);
            if length > 8192 {
                return Err("Request body exceeds limit".into());
            }
            let method = parser.method.ok_or("Missing HTTP method")?.to_string();
            let path = parser.path.ok_or("Missing HTTP path")?.to_string();
            while bytes.len() < offset + length {
                if Instant::now() >= deadline {
                    return Err("Body deadline".into());
                }
                let mut chunk = [0; 2048];
                match socket.read(&mut chunk) {
                    Ok(0) => return Err("Truncated body".into()),
                    Ok(n) => bytes.extend_from_slice(&chunk[..n]),
                    Err(e)
                        if matches!(
                            e.kind(),
                            std::io::ErrorKind::TimedOut | std::io::ErrorKind::WouldBlock
                        ) => {}
                    Err(e) => return Err(e.to_string()),
                }
            }
            return Ok(Request {
                method,
                path,
                headers: map,
                body: bytes[offset..offset + length].to_vec(),
            });
        }
        let mut chunk = [0; 2048];
        match socket.read(&mut chunk) {
            Ok(0) => return Err("Closed request".into()),
            Ok(n) => bytes.extend_from_slice(&chunk[..n]),
            Err(e)
                if matches!(
                    e.kind(),
                    std::io::ErrorKind::TimedOut | std::io::ErrorKind::WouldBlock
                ) => {}
            Err(e) => return Err(e.to_string()),
        }
    }
}
fn write_response(
    socket: &mut TcpStream,
    code: u16,
    kind: &str,
    body: &[u8],
) -> Result<(), String> {
    socket
        .set_write_timeout(Some(Duration::from_secs(2)))
        .map_err(|e| e.to_string())?;
    write!(socket,"HTTP/1.1 {code} Result\r\nContent-Type: {kind}\r\nContent-Length: {}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",body.len()).map_err(|e|e.to_string())?;
    socket.write_all(body).map_err(|e| e.to_string())
}
fn serve(mut socket: TcpStream, shared: &Shared) -> Result<(), String> {
    // Peek leaves the complete handshake for tungstenite's reviewed parser.
    socket
        .set_read_timeout(Some(Duration::from_secs(2)))
        .map_err(|e| e.to_string())?;
    socket
        .set_write_timeout(Some(Duration::from_secs(2)))
        .map_err(|e| e.to_string())?;
    let mut peek = [0; 16384];
    let started = Instant::now();
    let count = loop {
        let count = socket.peek(&mut peek).map_err(|e| e.to_string())?;
        if count == 0 {
            return Ok(());
        }
        if peek[..count].windows(4).any(|v| v == b"\r\n\r\n") {
            break count;
        }
        if count == peek.len() || started.elapsed() > Duration::from_secs(3) {
            return Err("Incomplete/oversized HTTP header".into());
        }
        thread::sleep(Duration::from_millis(2));
    };
    if std::str::from_utf8(&peek[..count])
        .is_ok_and(|s| s.to_ascii_lowercase().contains("upgrade: websocket"))
    {
        let mut limits = tungstenite::protocol::WebSocketConfig::default();
        limits.max_message_size = Some(8192);
        limits.max_frame_size = Some(8192);
        let mut ws = tungstenite::accept_hdr_with_config(
            socket,
            |r: &tungstenite::handshake::server::Request,
             response: tungstenite::handshake::server::Response| {
                let allowed = r.uri().path() == "/ws"
                    && !r.headers().contains_key("origin")
                    && trace(
                        r.headers()
                            .get("x-pakar-route")
                            .and_then(|v| v.to_str().ok()),
                        &shared.settings.instance_id,
                    )
                    .is_ok();
                if allowed {
                    Ok(response)
                } else {
                    Err(tungstenite::http::Response::builder()
                        .status(403)
                        .body(Some("Bridge request rejected".into()))
                        .unwrap())
                }
            },
            Some(limits),
        )
        .map_err(|e| e.to_string())?;
        ws.get_mut()
            .set_read_timeout(Some(Duration::from_millis(40)))
            .map_err(|e| e.to_string())?;
        let mut previous = Value::Null;
        let mut pushed = Instant::now() - Duration::from_secs(3);
        while !shared.stop.load(Ordering::Acquire) {
            let value=route(shared).map(|o|status_value(shared,&o)).unwrap_or_else(|_|json!({"type":"bridge-offline","bridge":{"instance_id":shared.settings.instance_id}}));
            let mut comparison = value.clone();
            comparison["command_clock_ms"] = Value::Null;
            if comparison != previous || pushed.elapsed() >= Duration::from_secs(2) {
                ws.send(tungstenite::Message::Text(value.to_string().into()))
                    .map_err(|e| e.to_string())?;
                previous = comparison;
                pushed = Instant::now();
            }
            match ws.read() {
                Ok(tungstenite::Message::Text(text)) if text.len() <= 8192 => {
                    let request: Value = serde_json::from_str(&text).map_err(|e| e.to_string())?;
                    if request["type"] == "command" {
                        ws.send(tungstenite::Message::Text(
                            command(shared, request, &BTreeMap::new())
                                .to_string()
                                .into(),
                        ))
                        .map_err(|e| e.to_string())?;
                    } else if request["type"] == "status" {
                        previous = Value::Null;
                    }
                }
                Ok(tungstenite::Message::Close(_)) => break,
                Err(tungstenite::Error::Io(e))
                    if matches!(
                        e.kind(),
                        std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut
                    ) => {}
                Err(_) => break,
                _ => {}
            }
        }
        return Ok(());
    }
    let request = match read_request(&mut socket) {
        Ok(r) => r,
        Err(e) => {
            return write_response(
                &mut socket,
                400,
                "application/json",
                json!({"error":e}).to_string().as_bytes(),
            );
        }
    };
    let path = request.path.split('?').next().unwrap_or("");
    if trace(
        request.headers.get("x-pakar-route").map(String::as_str),
        &shared.settings.instance_id,
    )
    .is_err()
    {
        return write_response(
            &mut socket,
            508,
            "application/json",
            b"{\"error\":\"Bridge loop or hop limit\"}",
        );
    }
    if request.method == "GET" && path == "/api/auth/info" {
        // This describes this listener, not an upstream board's login.
        return write_response(
            &mut socket,
            200,
            "application/json",
            br#"{"enabled":false,"ready":true,"username":"","default_credentials":false}"#,
        );
    }
    if request.method == "GET" && matches!(path, "/description.xml" | "/upnp/device.xml") {
        let xml = format!(
            "<root><device><deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType><friendlyName>{}</friendlyName><manufacturer>AtomicDeploy</manufacturer><modelName>PakarController</modelName><modelNumber>bridge</modelNumber><modelURL>https://github.com/atomicdeploy/PakarController</modelURL><UDN>uuid:{}</UDN></device></root>",
            shared.settings.hostname, shared.settings.instance_id
        );
        return write_response(&mut socket, 200, "text/xml", xml.as_bytes());
    }
    if request.method == "GET"
        && matches!(path, "/api/discovery" | "/upnp/public.json" | "/healthz")
    {
        let public = json!({"product":"PakarController","status":"ok","kind":"bridge","instance_id":shared.settings.instance_id,"hostname":shared.settings.hostname,"api":"/api","ws":"/ws","auth_required":false,"connected":route(shared).is_ok()});
        return write_response(
            &mut socket,
            200,
            "application/json",
            public.to_string().as_bytes(),
        );
    }
    let result = (|| -> Result<Value, String> {
        let o = route(shared)?;
        match (request.method.as_str(), path) {
            ("GET", "/api/status" | "/info") => Ok(status_value(shared, &o)),
            ("GET", "/api/cells/mapping") => {
                if o.mapping.is_null() {
                    Err("Current peer mappings unavailable".into())
                } else {
                    Ok(o.mapping)
                }
            }
            ("GET", path) if path.starts_with("/api/") && !path.contains("..") => match &o.route {
                Route::Network {
                    endpoint,
                    stamp,
                    gate,
                } => {
                    let _lease = gate
                        .acquire(Transport::WiFi, stamp.epoch)
                        .ok_or("Owner retired")?;
                    let client = peer_http::Pin::from_status(&o.status)
                        .ok_or("Peer unavailable")?
                        .client()?;
                    let response = client
                        .get(format!(
                            "{}{}",
                            endpoint.base.trim_end_matches('/'),
                            request.path
                        ))
                        .basic_auth(&endpoint.user, Some(&endpoint.password))
                        .header(
                            "X-Pakar-Route",
                            trace(
                                request.headers.get("x-pakar-route").map(String::as_str),
                                &shared.settings.instance_id,
                            )?,
                        )
                        .send()
                        .map_err(|e| e.to_string())?;
                    if !stamp.current() || !response.status().is_success() {
                        return Err("Upstream read unavailable or peer retired".into());
                    }
                    response
                        .json()
                        .map_err(|_| "Upstream did not return JSON".into())
                }
                _ => Err("API unavailable on the selected USB route".into()),
            },
            ("POST", "/api/command") => {
                Ok(command(shared, command_packet(&request)?, &request.headers))
            }
            ("POST", path) if path.starts_with("/api/") => {
                let body =
                    std::str::from_utf8(&request.body).map_err(|_| "Expected URL-encoded form")?;
                let url = reqwest::Url::parse(&format!("http://bridge/?{body}"))
                    .map_err(|_| "Invalid form")?;
                let form: Vec<_> = url
                    .query_pairs()
                    .map(|(k, v)| (k.into_owned(), v.into_owned()))
                    .collect();
                if form.len() > 16
                    || form.iter().enumerate().any(|(i, (k, v))| {
                        v.len() > 512 || form[..i].iter().any(|(key, _)| key == k)
                    })
                {
                    return Err("Invalid or duplicate form field".into());
                }
                mutate(
                    shared,
                    &o,
                    path,
                    form,
                    &request.headers,
                    &request.body,
                    Duration::from_secs(8),
                )
            }
            _ => Err("API not implemented on this route".into()),
        }
    })();
    let (code, value) = match result {
        Ok(v) => (200, v),
        Err(e) => (409, json!({"accepted":false,"error":e,"replayed":false})),
    };
    write_response(
        &mut socket,
        code,
        "application/json",
        value.to_string().as_bytes(),
    )
}

fn command_packet(request: &Request) -> Result<Value, String> {
    if request
        .headers
        .get("content-type")
        .is_some_and(|v| v.split(';').next() == Some("application/json"))
    {
        return serde_json::from_slice(&request.body).map_err(|_| "Invalid command JSON".into());
    }
    let body = std::str::from_utf8(&request.body).map_err(|_| "Invalid command form")?;
    let url = reqwest::Url::parse(&format!("http://bridge/?{body}"))
        .map_err(|_| "Invalid command form")?;
    let fields = url.query_pairs().collect::<Vec<_>>();
    if fields.len() != 1 || fields[0].0 != "packet" {
        return Err("Expected one packet field".into());
    }
    serde_json::from_str(&fields[0].1).map_err(|_| "Invalid command packet".into())
}
#[cfg(test)]
mod tests {
    use super::*;
    fn fixture() -> (Arc<Shared>, mpsc::Receiver<SerialCommand>, connection::Gate) {
        let settings = Settings {
            hostname: "fixture-host".into(),
            listen: "127.0.0.1".into(),
            ..Default::default()
        };
        let gate = connection::Gate::default();
        let epoch = gate.select(Some(Transport::Serial));
        let (tx, rx) = mpsc::channel();
        let status:DeviceStatus=serde_json::from_value(json!({"hostname":"ledboard","board_id":"fixture-board","boot_id":"a".repeat(32),
            "http_peer_fencing":true,"command_clock_ms":1000,"cells":vec!["white";80],"pixels":480,"mode":"cell",
            "capabilities":{"board_kind":"table","acknowledged_features":["table","leds","cell-batch","command-ack","ota"],"table":{"columns":10,"rows":8,"pixels_per_cell":6}}})).unwrap();
        let admission = usb_table::Admission {
            stamp: gate.stamp(Transport::Serial, epoch),
            session: 1,
            peer: peer_http::Pin::from_status(&status),
            at: Instant::now(),
            batch: None,
        };
        (
            Arc::new(Shared {
                settings,
                observation: Mutex::new(Some(Observation {
                    status,
                    mapping: json!({"physical":(1..=80).collect::<Vec<_>>()}),
                    route: Route::Usb {
                        port: "fixture".into(),
                        admission,
                        sender: tx,
                    },
                    at: Instant::now(),
                })),
                stop: Arc::new(AtomicBool::new(false)),
                clients: AtomicUsize::new(0),
                receipts: Mutex::new(BTreeMap::new()),
                last_input: Mutex::new(Value::Null),
            }),
            rx,
            gate,
        )
    }
    #[test]
    fn bridge_stop_revokes_queued_requests_without_releasing_primary_usb_owner() {
        let (shared, _, _) = fixture();
        let o = route(&shared).unwrap();
        let Route::Usb { admission, .. } = o.route else {
            unreachable!()
        };
        let (tx, rx) = mpsc::channel();
        let intent = usb_table::Intent {
            opcode: 0x4c,
            payload: vec![1, 1, 0],
            admission: admission.clone(),
            reply: Some(Reply::new(tx, shared.stop.clone())),
        };
        assert!(intent.current(1, Some(&o.status)));
        shared.stop.store(true, Ordering::Release);
        assert!(admission.stamp.current()); // Local board remains selected.
        assert!(!intent.current(1, Some(&o.status)));
        intent.reply(Ok(o.status));
        assert!(rx.recv().unwrap().is_err());
    }
    #[test]
    fn bridge_http_recovery_uses_existing_packet_form_contract() {
        let packet = json!({"type":"command","request_id":"recovery-id"});
        let mut url = reqwest::Url::parse("http://fixture/").unwrap();
        url.query_pairs_mut()
            .append_pair("packet", &packet.to_string());
        let mut request = Request {
            method: "POST".into(),
            path: "/api/command".into(),
            headers: BTreeMap::new(),
            body: url.query().unwrap().as_bytes().to_vec(),
        };
        assert_eq!(command_packet(&request).unwrap(), packet);
        request.body.extend(b"&packet=%7B%7D");
        assert!(command_packet(&request).is_err());
        request.body = packet.to_string().into_bytes();
        request
            .headers
            .insert("content-type".into(), "application/json".into());
        assert_eq!(command_packet(&request).unwrap(), packet);
    }
    #[test]
    fn bridge_trace_and_observation_loops_fail_closed() {
        let a = uuid::Uuid::new_v4().to_string();
        let b = uuid::Uuid::new_v4().to_string();
        assert_eq!(trace(None, &a).unwrap(), a);
        assert!(trace(Some(&a), &a).is_err());
        assert!(trace(Some("not-an-instance"), &a).is_err());
        assert_eq!(trace(Some(&a), &b).unwrap(), format!("{a},{b}"));
        let (shared, _, _) = fixture();
        let mut o = shared.observation.lock().unwrap();
        o.as_mut().unwrap().status.bridge = Some(json!({"path":[shared.settings.instance_id]}));
        drop(o);
        assert!(route(&shared).is_err());
    }
    #[test]
    fn bridge_status_keeps_board_identity_and_only_available_route_capabilities() {
        let (shared, _, gate) = fixture();
        let o = route(&shared).unwrap();
        let v = status_value(&shared, &o);
        assert_eq!(v["board_id"], "fixture-board");
        assert_eq!(v["hostname"], "ledboard");
        assert_eq!(v["bridge"]["hostname"], "fixture-host");
        assert!(
            !v["capabilities"]["acknowledged_features"]
                .as_array()
                .unwrap()
                .contains(&json!("ota"))
        );
        assert_eq!(v["bridge"]["path"], json!([shared.settings.instance_id]));
        gate.select(None);
        assert!(route(&shared).is_err());
    }
    #[test]
    fn bridge_same_command_id_returns_correlated_readback_without_second_usb_write() {
        let (shared, rx, _) = fixture();
        let before = route(&shared).unwrap().status;
        let worker = thread::spawn(move || {
            if let SerialCommand::Bridge(_, admission, op, payload, reply) =
                rx.recv_timeout(Duration::from_secs(2)).unwrap()
            {
                assert_eq!(op, 0x4c);
                assert_eq!(payload, [1, 1, 0]);
                assert!(admission.current(1, Some(&before)));
                let mut actual = before;
                actual.cells[0] = "red".into();
                reply.send(Ok(actual)).unwrap();
            } else {
                panic!("Only the selected USB worker may receive this command")
            };
            assert!(rx.recv_timeout(Duration::from_millis(80)).is_err());
        });
        let request = json!({"type":"command","request_id":"one-intent","board_id":"fixture-board","boot_id":"a".repeat(32),
            "expires_at_ms":9000,"command":"cell","id":1,"state":"red","effect":"immediate"});
        let first = command(&shared, request.clone(), &BTreeMap::new());
        assert_eq!(first["accepted"], true);
        assert_eq!(first["status"]["cells"][0], "red");
        let second = command(&shared, request.clone(), &BTreeMap::new());
        assert_eq!(first, second);
        let mut foreign = request;
        foreign["id"] = json!(2);
        assert_eq!(
            command(&shared, foreign, &BTreeMap::new())["accepted"],
            false
        );
        worker.join().unwrap();
    }
    #[test]
    fn bridge_rejects_expired_wrong_peer_and_raw_unfenced_mutations_before_usb() {
        let (shared, rx, _) = fixture();
        let o = route(&shared).unwrap();
        assert!(
            mutate(
                &shared,
                &o,
                "/api/cell",
                vec![],
                &BTreeMap::new(),
                &[],
                Duration::from_secs(8)
            )
            .is_err()
        );
        for request in [
            json!({"type":"command","request_id":"old","board_id":"fixture-board","boot_id":"a".repeat(32),"expires_at_ms":1}),
            json!({"type":"command","request_id":"foreign","board_id":"other","boot_id":"a".repeat(32),"expires_at_ms":9000}),
        ] {
            assert_eq!(
                command(&shared, request, &BTreeMap::new())["accepted"],
                false
            );
        }
        assert!(rx.try_recv().is_err());
        assert!(usb_request("/api/firmware/apply", &[], &o.status).is_err());
    }
    #[test]
    fn bridge_hostname_matches_bridge_without_renaming_its_downstream_board() {
        let (shared, _, _) = fixture();
        let o = route(&shared).unwrap();
        let mut s: DeviceStatus = serde_json::from_value(status_value(&shared, &o)).unwrap();
        assert!(matches_hostname(&s, "fixture-host"));
        assert!(matches_hostname(&s, "ledboard"));
        assert!(!matches_hostname(&s, "foreign"));
        s.bridge.as_mut().unwrap()["path"] =
            json!([shared.settings.instance_id, shared.settings.instance_id]);
        assert!(!matches_hostname(&s, "fixture-host"));
    }
    #[test]
    fn bridge_network_repeater_confirms_upstream_usb_readback_and_preserves_both_hops() {
        let (upstream, rx, _) = fixture();
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let peer = upstream.clone();
        let server = thread::spawn(move || {
            for _ in 0..2 {
                serve(listener.accept().unwrap().0, &peer).unwrap();
            }
        });
        let usb = thread::spawn(move || {
            let SerialCommand::Bridge(_, _, op, payload, reply) =
                rx.recv_timeout(Duration::from_secs(2)).unwrap()
            else {
                panic!("Not the selected USB owner");
            };
            assert_eq!((op, payload), (0x4c, vec![1, 1, 0]));
            let mut s = route(&upstream).unwrap().status;
            s.cells[0] = "red".into();
            reply.send(Ok(s)).unwrap();
        });
        let (downstream, _, _) = fixture();
        let gate = connection::Gate::default();
        let epoch = gate.select(Some(Transport::WiFi));
        {
            let mut o = downstream.observation.lock().unwrap();
            o.as_mut().unwrap().route = Route::Network {
                endpoint: Endpoint {
                    base: format!("http://{address}"),
                    user: String::new(),
                    password: String::new(),
                },
                stamp: gate.stamp(Transport::WiFi, epoch),
                gate: gate.clone(),
            };
        }
        let request = json!({"type":"command","request_id":"repeater-command","board_id":"fixture-board","boot_id":"a".repeat(32),"expires_at_ms":7500,"command":"cell","id":1,"state":"red","effect":"immediate"});
        let result = command(&downstream, request.clone(), &BTreeMap::new());
        assert_eq!(result["accepted"], true, "{result}");
        assert_eq!(result["status"]["cells"][0], "red");
        assert_eq!(
            result["status"]["bridge"]["path"].as_array().unwrap().len(),
            2
        );
        assert_eq!(command(&downstream, request, &BTreeMap::new()), result);
        gate.select(None);
        assert!(route(&downstream).is_err());
        usb.join().unwrap();
        server.join().unwrap();
    }
    #[test]
    fn bridge_real_http_and_websocket_share_the_same_board_snapshot() {
        let (shared, _, _) = fixture();
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let owner = shared.clone();
        let worker = thread::spawn(move || {
            for _ in 0..3 {
                let (socket, _) = listener.accept().unwrap();
                serve(socket, &owner).unwrap();
            }
        });
        let client = reqwest::blocking::Client::builder()
            .no_proxy()
            .build()
            .unwrap();
        let value: Value = client
            .get(format!("http://{address}/api/status"))
            .send()
            .unwrap()
            .json()
            .unwrap();
        assert_eq!(value["board_id"], "fixture-board");
        let auth: Value = client
            .get(format!("http://{address}/api/auth/info"))
            .send()
            .unwrap()
            .json()
            .unwrap();
        assert_eq!(auth["enabled"], false);
        let (mut ws, _) = tungstenite::connect(format!("ws://{address}/ws")).unwrap();
        let value: Value = serde_json::from_str(ws.read().unwrap().to_text().unwrap()).unwrap();
        assert_eq!(value["board_id"], "fixture-board");
        ws.close(None).unwrap();
        drop(ws);
        worker.join().unwrap();
    }
    #[test]
    fn bridge_parser_rejects_duplicate_length_and_browser_origin() {
        for headers in [
            "Content-Length: 0\r\nContent-Length: 0\r\n",
            "Origin: http://foreign\r\n",
            "Transfer-Encoding: chunked\r\n",
        ] {
            let listener = TcpListener::bind("127.0.0.1:0").unwrap();
            let address = listener.local_addr().unwrap();
            let task = thread::spawn(move || read_request(&mut listener.accept().unwrap().0));
            let mut socket = TcpStream::connect(address).unwrap();
            write!(
                socket,
                "POST /api/cell HTTP/1.1\r\nHost: fixture\r\n{headers}\r\n"
            )
            .unwrap();
            assert!(task.join().unwrap().is_err());
        }
    }
}

impl crate::PakarControllerApp {
    pub(crate) fn bridge_tick(&mut self) {
        self.bridge
            .reconcile(&self.config.bridge, self.screenshot_harness);
        let observation = self.live_status().cloned().and_then(|status| {
            let route = match self.command_route()? {
                Transport::Serial => Route::Usb {
                    port: self.serial_port.clone(),
                    admission: self.serial_admission()?,
                    sender: self.serial_tx.clone(),
                },
                Transport::WiFi => Route::Network {
                    endpoint: self.endpoint(),
                    stamp: self
                        .connection_gate
                        .stamp(Transport::WiFi, self.connection_gate.epoch()),
                    gate: self.connection_gate.clone(),
                },
            };
            let at = match self.transport {
                Transport::Serial => self.last_serial_status_at?,
                Transport::WiFi => self.last_status_at?,
            };
            let mapping = if self.mapping_peer == peer_http::Pin::from_status(&status)
                && self.mappings.len() == 80
            {
                json!({"board_id":status.board_id,"boot_id":status.boot_id,
                    "physical":self.mappings.iter().map(|c|c.physical_cell).collect::<Vec<_>>(),
                    "names":self.mappings.iter().map(|c|c.name.clone()).collect::<Vec<_>>()})
            } else {
                Value::Null
            };
            Some(Observation {
                status,
                mapping,
                route,
                at,
            })
        });
        self.bridge.observe(observation);
    }
}
