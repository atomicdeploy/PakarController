//! Adapted PCController hostbridge/manager.go's bounded outbound WS/Socket.IO
//! sessions. This only delivers pinned firmware offers, never arbitrary RPC.
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use std::{
    collections::HashSet,
    net::{TcpStream, ToSocketAddrs},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
        mpsc::{self, Receiver},
    },
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};
use tungstenite::{Message, client::IntoClientRequest, stream::MaybeTlsStream};

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub enum Protocol {
    #[default]
    WebSocket,
    SocketIo,
}
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub endpoint: String,
    pub protocol: Protocol,
    pub namespace: String,
    pub event: String,
    pub download_origin: String,
    pub deliver: bool,
}
impl Default for Settings {
    fn default() -> Self {
        Self {
            endpoint: String::new(),
            protocol: Protocol::WebSocket,
            namespace: "/".into(),
            event: "firmware-offer".into(),
            download_origin: String::new(),
            deliver: false,
        }
    }
}
impl Settings {
    pub fn validate(&self) -> Result<(), String> {
        let u =
            reqwest::Url::parse(&self.endpoint).map_err(|_| "Enter a remote WebSocket endpoint")?;
        if self.endpoint.len() > 2048
            || !matches!(u.scheme(), "ws" | "wss")
            || u.host_str().is_none()
            || !u.username().is_empty()
            || u.password().is_some()
            || u.fragment().is_some()
            || u.query().is_some()
            || self
                .endpoint
                .bytes()
                .any(|b| b <= 32 || b >= 127 || b == b'\\')
            || (u.scheme() == "ws"
                && !matches!(u.host_str(), Some("localhost" | "127.0.0.1" | "[::1]")))
        {
            return Err("Use wss:// with no credentials/query; plaintext is allowed only for an explicit loopback test".into());
        }
        if !self.namespace.starts_with('/')
            || self.namespace.len() > 96
            || self
                .namespace
                .bytes()
                .any(|b| !(b.is_ascii_alphanumeric() || b"/_-".contains(&b)))
            || self.event.is_empty()
            || self.event.len() > 64
            || self
                .event
                .bytes()
                .any(|b| !(b.is_ascii_alphanumeric() || b"._-".contains(&b)))
        {
            return Err("Invalid namespace or exact firmware event".into());
        }
        let d = crate::firmware_source::url(&self.download_origin)?;
        if d.scheme() != "https" || d.path() != "/" || d.query().is_some() {
            return Err("Set an exact trusted HTTPS download origin, without a file path".into());
        }
        Ok(())
    }
}
#[derive(Clone, Debug, Deserialize)]
pub struct Offer {
    pub id: String,
    pub session: String,
    pub board_id: String,
    pub boot_id: String,
    pub expires_at_ms: u64,
    pub url: String,
    pub sha256: String,
    pub target: String,
    pub kind: String,
}
impl Offer {
    fn validate(
        &self,
        settings: &Settings,
        session: &str,
        board: &str,
        boot: &str,
        now: u64,
    ) -> Result<(), String> {
        let source = crate::firmware_source::url(&self.url)?;
        let trusted = crate::firmware_source::url(&settings.download_origin)?;
        if self.id.is_empty()
            || self.id.len() > 64
            || !self
                .id
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || b"_-".contains(&b))
            || self.session != session
            || self.board_id != board
            || self.boot_id != boot
            || self.expires_at_ms <= now
            || self.expires_at_ms > now.saturating_add(60000)
            || source.origin() != trusted.origin()
            || !crate::maintenance::valid_hash(&self.sha256)
            || !matches!(self.target.as_str(), "esp" | "avr" | "all")
            || !matches!(self.kind.as_str(), "binary" | "manifest")
            || self.kind == "binary" && self.target == "all"
        {
            return Err(
                "Rejected stale, wrong-peer/session/origin or invalid firmware offer".into(),
            );
        }
        Ok(())
    }
}
pub enum Event {
    Connected,
    Offer(Offer),
    Stopped(String),
}
pub struct Session {
    stop: Arc<AtomicBool>,
    rx: Receiver<Event>,
    pub session: String,
    pub board: String,
    pub boot: String,
    pub state: String,
    pub pending: Option<Offer>,
    pub settings: Settings,
}
impl Drop for Session {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Release);
    }
}
impl Session {
    pub fn start(
        settings: Settings,
        token: String,
        board: String,
        boot: String,
    ) -> Result<Self, String> {
        settings.validate()?;
        if token.len() > 256 || token.bytes().any(|b| !(33..=126).contains(&b)) {
            return Err("Invalid feed bearer".into());
        }
        let session = uuid::Uuid::new_v4().simple().to_string();
        let stop = Arc::new(AtomicBool::new(false));
        let (tx, rx) = mpsc::sync_channel(8);
        let (cfg, secret, b, boot_id, identity, cancel) = (
            settings.clone(),
            token,
            board.clone(),
            boot.clone(),
            session.clone(),
            stop.clone(),
        );
        std::thread::Builder::new()
            .name("pakar-firmware-feed".into())
            .spawn(move || {
                let result = listen(&cfg, &secret, &b, &boot_id, &identity, &cancel, |e| {
                    tx.try_send(e).map_err(|_| {
                        "Firmware inbox full; listener stopped, nothing replayed".to_owned()
                    })
                });
                if !cancel.load(Ordering::Acquire) {
                    let _ =
                        tx.try_send(Event::Stopped(result.err().unwrap_or_else(|| {
                            "Remote feed closed; offers are not replayed".into()
                        })));
                }
                cancel.store(true, Ordering::Release);
            })
            .map_err(|_| "Cannot start firmware listener")?;
        Ok(Self {
            stop,
            rx,
            session,
            board,
            boot,
            state: "Connecting".into(),
            pending: None,
            settings,
        })
    }
    pub fn drain(&mut self) -> Vec<Offer> {
        let mut offers = Vec::new();
        while let Ok(e) = self.rx.try_recv() {
            match e {
                Event::Connected => self.state = "Listening · subscription sent".into(),
                Event::Offer(v) => offers.push(v),
                Event::Stopped(s) => {
                    self.state = s;
                    self.stop.store(true, Ordering::Release);
                }
            }
        }
        offers
    }
}
pub fn now_ms() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}

// Socket.IO v4 clients use Engine.IO EIO=4 and Socket.IO protocol5. These are
// standards-required wire markers, never a versioned Pakar API.
pub struct Wire {
    settings: Settings,
    open: bool,
    connected: bool,
    pub heartbeat: Duration,
}
pub enum Packet {
    Send(String),
    Connected,
    Offer(Value),
    Ignore,
}
impl Wire {
    pub fn new(s: Settings) -> Self {
        Self {
            settings: s,
            open: false,
            connected: false,
            heartbeat: Duration::from_secs(15),
        }
    }
    fn ns(&self) -> String {
        if self.settings.namespace == "/" {
            String::new()
        } else {
            format!("{},", self.settings.namespace)
        }
    }
    pub fn event(&self, name: &str, v: Value) -> String {
        format!("42{}{}", self.ns(), json!([name, v]))
    }
    pub fn receive(&mut self, text: &str, token: &str) -> Result<Packet, String> {
        if text.len() > 8192 {
            return Err("Oversized firmware event".into());
        }
        if self.settings.protocol == Protocol::WebSocket {
            let v: Value = serde_json::from_str(text).map_err(|_| "Invalid firmware feed JSON")?;
            return Ok(if v["type"] == self.settings.event {
                Packet::Offer(v)
            } else {
                Packet::Ignore
            });
        }
        if text.starts_with('0') && !self.open {
            let v: Value =
                serde_json::from_str(&text[1..]).map_err(|_| "Invalid Engine.IO open")?;
            let interval = v["pingInterval"]
                .as_u64()
                .filter(|v| (1..=120000).contains(v))
                .ok_or("Invalid heartbeat interval")?;
            let timeout = v["pingTimeout"]
                .as_u64()
                .filter(|v| (1..=60000).contains(v))
                .ok_or("Invalid heartbeat timeout")?;
            if v["sid"]
                .as_str()
                .is_none_or(|s| s.is_empty() || s.len() > 256)
            {
                return Err("Missing Engine.IO session".into());
            }
            self.heartbeat = Duration::from_millis(interval + timeout);
            self.open = true;
            let auth = if token.is_empty() {
                String::new()
            } else {
                json!({"token":token}).to_string()
            };
            return Ok(Packet::Send(format!("40{}{}", self.ns(), auth)));
        }
        if !self.open {
            return Err("Socket.IO event before Engine.IO open".into());
        }
        if let Some(ping) = text.strip_prefix('2') {
            return Ok(Packet::Send(format!("3{ping}")));
        }
        let connect = format!("40{}", self.ns());
        if !self.connected
            && (text == connect
                || text
                    .strip_prefix(&connect)
                    .is_some_and(|v| v.starts_with('{')))
        {
            self.connected = true;
            return Ok(Packet::Connected);
        }
        if text.starts_with('1')
            || text.starts_with(&format!("41{}", self.ns()))
            || text.starts_with(&format!("44{}", self.ns()))
        {
            return Err("Remote Socket.IO session refused/closed".into());
        }
        let Some(raw) = text.strip_prefix(&format!("42{}", self.ns())) else {
            return Ok(Packet::Ignore);
        };
        if !self.connected {
            return Err("Firmware event before namespace acknowledgment".into());
        }
        // Optional event ACK ID precedes the JSON. Never confuse it with data.
        let raw = raw.trim_start_matches(|c: char| c.is_ascii_digit());
        if !raw.starts_with('[') {
            return Ok(Packet::Ignore);
        }
        let v: Value = serde_json::from_str(raw).map_err(|_| "Invalid Socket.IO event")?;
        let a = v
            .as_array()
            .filter(|a| a.len() == 2)
            .ok_or("Expected exact event/data tuple")?;
        Ok(if a[0] == self.settings.event {
            Packet::Offer(a[1].clone())
        } else {
            Packet::Ignore
        })
    }
}
fn listen(
    s: &Settings,
    token: &str,
    board: &str,
    boot: &str,
    session: &str,
    stop: &AtomicBool,
    mut emit: impl FnMut(Event) -> Result<(), String>,
) -> Result<(), String> {
    let mut target = reqwest::Url::parse(&s.endpoint).map_err(|_| "Invalid feed URL")?;
    if s.protocol == Protocol::SocketIo {
        target
            .query_pairs_mut()
            .append_pair("EIO", "4")
            .append_pair("transport", "websocket");
    }
    let mut request = target
        .as_str()
        .into_client_request()
        .map_err(|_| "Invalid feed handshake")?;
    if !token.is_empty() {
        request.headers_mut().insert(
            "Authorization",
            format!("Bearer {token}")
                .parse()
                .map_err(|_| "Invalid feed bearer")?,
        );
    }
    let addresses = (
        target.host_str().ok_or("Missing feed host")?,
        target
            .port_or_known_default()
            .unwrap_or(if target.scheme() == "wss" { 443 } else { 80 }),
    )
        .to_socket_addrs()
        .map_err(|_| "Feed DNS lookup failed")?;
    let mut tcp = None;
    for addr in addresses.take(4) {
        if stop.load(Ordering::Acquire) {
            return Ok(());
        }
        if let Ok(s) = TcpStream::connect_timeout(&addr, Duration::from_secs(3)) {
            tcp = Some(s);
            break;
        }
    }
    let tcp = tcp.ok_or("Feed TCP connection failed")?;
    tcp.set_read_timeout(Some(Duration::from_secs(8)))
        .map_err(|_| "Cannot bound feed handshake")?;
    tcp.set_write_timeout(Some(Duration::from_secs(8)))
        .map_err(|_| "Cannot bound feed writes")?;
    let cfg = tungstenite::protocol::WebSocketConfig::default()
        .max_message_size(Some(8192))
        .max_frame_size(Some(8192));
    let (mut ws, _) = tungstenite::client_tls_with_config(request, tcp, Some(cfg), None)
        .map_err(|_| "Feed TLS/WebSocket handshake failed; no insecure fallback")?;
    match ws.get_mut() {
        MaybeTlsStream::Plain(s) => s.set_read_timeout(Some(Duration::from_millis(250))),
        MaybeTlsStream::Rustls(s) => s.sock.set_read_timeout(Some(Duration::from_millis(250))),
        _ => return Err("Unsupported secure feed stream".into()),
    }
    .map_err(|_| "Cannot bound feed reads")?;
    let mut wire = Wire::new(s.clone());
    let mut seen = HashSet::new();
    let mut last = Instant::now();
    let mut last_ping = Instant::now();
    let sub = json!({"type":"subscribe","events":[s.event],"session":session,"board_id":board,"boot_id":boot});
    if s.protocol == Protocol::WebSocket {
        ws.send(Message::Text(sub.to_string().into()))
            .map_err(|_| "Subscription write failed")?;
        emit(Event::Connected)?;
    }
    while !stop.load(Ordering::Acquire) {
        match ws.read() {
            Ok(Message::Text(t)) => {
                last = Instant::now();
                match wire.receive(&t, token)? {
                    Packet::Send(p) => ws
                        .send(Message::Text(p.into()))
                        .map_err(|_| "Feed heartbeat/connect failed")?,
                    Packet::Connected => {
                        ws.send(Message::Text(wire.event("subscribe", sub.clone()).into()))
                            .map_err(|_| "Subscription failed")?;
                        emit(Event::Connected)?;
                    }
                    Packet::Offer(v) => {
                        let offer: Offer =
                            serde_json::from_value(v).map_err(|_| "Invalid firmware offer")?;
                        offer.validate(s, session, board, boot, now_ms())?;
                        if seen.contains(&offer.id) {
                            continue;
                        }
                        if seen.len() >= 1024 {
                            return Err(
                                "Firmware session budget reached; explicitly restart listener"
                                    .into(),
                            );
                        }
                        seen.insert(offer.id.clone());
                        emit(Event::Offer(offer))?;
                    }
                    Packet::Ignore => {}
                }
            }
            Ok(Message::Ping(_)) => {
                ws.flush().map_err(|_| "Feed pong failed")?;
                last = Instant::now();
            }
            Ok(Message::Pong(_)) => last = Instant::now(),
            Ok(Message::Close(_)) => return Err("Remote feed closed; no offers replayed".into()),
            Ok(_) => {
                return Err(
                    "Binary firmware events are not supported; use a pinned HTTP file offer".into(),
                );
            }
            Err(tungstenite::Error::Io(e))
                if matches!(
                    e.kind(),
                    std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut
                ) =>
            {
                if last.elapsed() > wire.heartbeat.max(Duration::from_secs(30)) {
                    return Err("Remote feed heartbeat expired".into());
                }
                if s.protocol == Protocol::WebSocket
                    && last_ping.elapsed() > Duration::from_secs(20)
                {
                    ws.send(Message::Ping(Vec::new().into()))
                        .map_err(|_| "Feed ping failed")?;
                    last_ping = Instant::now();
                }
            }
            Err(_) => return Err("Remote feed disconnected; no offers replayed".into()),
        }
    }
    let _ = ws.close(None);
    Ok(())
}

impl crate::PakarControllerApp {
    pub fn source_operation(
        &self,
        source: crate::firmware_source::Source,
        esp: bool,
    ) -> Result<crate::maintenance::Operation, String> {
        if self.command_route() != Some(crate::Transport::WiFi) || !self.http_writable() {
            return Err("Select a live network update route; USB-only firmware delivery is not implemented and no Wi-Fi fallback was used".into());
        }
        let s = self.live_status().ok_or("Connect to a live board first")?;
        if !self.has_feature(if esp { "ota" } else { "isp" }) {
            return Err("Board has not acknowledged this updater".into());
        }
        Ok(crate::maintenance::Operation::SourceImage {
            source,
            esp,
            progress: crate::firmware_progress::Transfer::new(self.http_tx.capture(), s),
        })
    }
    pub fn firmware_observation(&self) -> Option<crate::firmware_progress::Observation> {
        let status = self.live_status().filter(|s| {
            s.capabilities
                .as_ref()
                .is_some_and(|c| c.has("ota") || c.has("isp"))
        });
        let local = self
            .firmware_transfer
            .as_ref()
            .and_then(|v| v.snapshot(status));
        let board = status.and_then(crate::firmware_progress::observed);
        local.filter(|v| v.active).or(board).or_else(|| {
            self.firmware_transfer
                .as_ref()
                .and_then(|v| v.snapshot(status))
        })
    }
    pub fn start_firmware_feed(&mut self, settings: Settings, token: String) -> Result<(), String> {
        if self.firmware_feed.is_some() {
            return Err("Stop the current listener before changing its settings".into());
        }
        let s = self
            .live_status()
            .ok_or("Connect to an acknowledged board first")?;
        if crate::peer_http::Pin::from_status(s).is_none() {
            return Err("Board identity is not pinned".into());
        }
        self.firmware_feed = Some(Session::start(
            settings.clone(),
            token,
            s.board_id.clone(),
            s.boot_id.clone(),
        )?);
        self.firmware_feed_message.clear();
        self.config.firmware_feed = settings;
        self.save_workspace();
        Ok(())
    }
    pub fn deliver_firmware_offer(&mut self, offer: Offer) -> Result<(), String> {
        let feed = self
            .firmware_feed
            .as_ref()
            .ok_or("Firmware listener was stopped")?;
        let status = self.live_status().ok_or("Board is not online")?;
        offer.validate(
            &feed.settings,
            &feed.session,
            &status.board_id,
            &status.boot_id,
            now_ms(),
        )?;
        if self.command_route() != Some(crate::Transport::WiFi)
            || !self.http_writable()
            || self.device_settings.maintenance_pending.is_some()
            || self.device_settings.pending.is_some()
            || self.device_settings.maintenance_confirmation.is_some()
            || self.device_settings.dirty()
            || status.remote_firmware["busy"] == true
            || status.remote_firmware["phase"] == "ready"
        {
            return Err("Firmware offer not dispatched: update route, draft or maintenance owner is unavailable. Nothing queued/replayed".into());
        }
        if (offer.target != "avr" && !self.has_feature("ota"))
            || (offer.target != "esp" && !self.has_feature("isp"))
        {
            return Err("Offer target capability not acknowledged".into());
        }
        let deadline = offer.expires_at_ms;
        let mut op = if offer.kind == "manifest" {
            crate::maintenance::validate_remote(
                &offer.url,
                &offer.sha256,
                if offer.target == "all" {
                    3
                } else if offer.target == "esp" {
                    1
                } else {
                    2
                },
            )?;
            crate::maintenance::Operation::RemoteStage {
                url: offer.url,
                sha256: offer.sha256,
                targets: if offer.target == "all" {
                    3
                } else if offer.target == "esp" {
                    1
                } else {
                    2
                },
                bearer: String::new(),
            }
        } else {
            self.source_operation(
                crate::firmware_source::Source::Http {
                    url: offer.url,
                    sha256: offer.sha256,
                    bearer: String::new(),
                },
                offer.target == "esp",
            )?
        };
        if let crate::maintenance::Operation::SourceImage { progress, .. } = &mut op {
            progress.dispatch_deadline = Some(deadline);
            self.firmware_transfer = Some(progress.clone());
        }
        let id = self.device_settings.next_id();
        self.http_tx
            .send(crate::HttpCommand::Maintenance(self.endpoint(), id, op))
            .map_err(|_| "Firmware queue closed; not replayed")?;
        self.device_settings.maintenance_pending = Some(id);
        self.firmware_feed_message =
            "Firmware offer dispatched once; follow measured/board progress".into();
        Ok(())
    }
    pub fn poll_firmware_feed(&mut self) {
        let peer = self
            .live_status()
            .map(|s| (s.board_id.clone(), s.boot_id.clone()));
        if self.firmware_feed.as_ref().is_some_and(|f| {
            peer.as_ref()
                .is_none_or(|(b, boot)| b != &f.board || boot != &f.boot)
        }) {
            self.firmware_feed = None;
            self.firmware_feed_message =
                "Listener stopped: selected board disconnected/changed; offers discarded".into();
            return;
        }
        let Some(feed) = self.firmware_feed.as_mut() else {
            return;
        };
        let auto = feed.settings.deliver;
        let offers = feed.drain();
        if !auto {
            for offer in offers {
                if feed.pending.is_none() {
                    feed.pending = Some(offer);
                } else {
                    self.firmware_feed_message =
                        "Additional offer discarded while one awaits a decision".into();
                }
            }
        } else {
            for offer in offers {
                if let Err(e) = self.deliver_firmware_offer(offer) {
                    self.firmware_feed_message = e;
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn firmware_feed_real_socketio_handshake_namespace_ping_and_event() {
        let mut s = Settings::default();
        s.protocol = Protocol::SocketIo;
        s.namespace = "/updates".into();
        let mut w = Wire::new(s);
        assert!(
            matches!(w.receive("0{\"sid\":\"server\",\"pingInterval\":25000,\"pingTimeout\":20000}","private").unwrap(),Packet::Send(p) if p=="40/updates,{\"token\":\"private\"}")
        );
        assert!(matches!(w.receive("2probe","").unwrap(),Packet::Send(p) if p=="3probe"));
        assert!(matches!(
            w.receive("40/other,{}", "").unwrap(),
            Packet::Ignore
        ));
        assert!(matches!(
            w.receive("40/updates,{\"sid\":\"ns\"}", "").unwrap(),
            Packet::Connected
        ));
        assert!(
            matches!(w.receive("42/updates,19[\"firmware-offer\",{\"id\":\"a\"}]","").unwrap(),Packet::Offer(v) if v["id"]=="a")
        );
        assert!(matches!(
            w.receive("42/other,[\"firmware-offer\",{}]", "").unwrap(),
            Packet::Ignore
        ));
        assert!(
            w.receive("44/updates,{\"message\":\"denied\"}", "")
                .is_err()
        );
    }
    #[test]
    fn firmware_feed_offers_require_fresh_exact_peer_session_origin_and_hash() {
        let s = Settings {
            endpoint: "wss://updates.example/events".into(),
            download_origin: "https://updates.example/".into(),
            ..Default::default()
        };
        assert!(s.validate().is_ok());
        let mut v = Offer {
            id: "offer".into(),
            session: "session".into(),
            board_id: "board".into(),
            boot_id: "boot".into(),
            expires_at_ms: 1100,
            url: "https://updates.example/esp.bin".into(),
            sha256: "a".repeat(64),
            target: "esp".into(),
            kind: "binary".into(),
        };
        assert!(v.validate(&s, "session", "board", "boot", 1000).is_ok());
        assert!(v.validate(&s, "other", "board", "boot", 1000).is_err());
        assert!(v.validate(&s, "session", "board", "boot", 1100).is_err());
        v.url = "https://other.example/esp.bin".into();
        assert!(v.validate(&s, "session", "board", "boot", 1000).is_err());
    }
    #[test]
    fn firmware_feed_actual_websocket_and_socketio_sessions_deliver_once() {
        use std::net::TcpListener;
        for socket_io in [false, true] {
            let server = TcpListener::bind("127.0.0.1:0").unwrap();
            let addr = server.local_addr().unwrap();
            let worker = std::thread::spawn(move || {
                let (tcp, _) = server.accept().unwrap();
                tcp.set_read_timeout(Some(Duration::from_secs(5))).unwrap();
                let mut ws = tungstenite::accept(tcp).unwrap();
                if socket_io {
                    ws.send(Message::Text(
                        "0{\"sid\":\"engine\",\"pingInterval\":25000,\"pingTimeout\":20000}".into(),
                    ))
                    .unwrap();
                    assert_eq!(ws.read().unwrap().to_text().unwrap(), "40/updates,");
                    ws.send(Message::Text("40/updates,{\"sid\":\"namespace\"}".into()))
                        .unwrap();
                    ws.send(Message::Text("2".into())).unwrap();
                }
                let packet = ws.read().unwrap();
                let raw = packet.to_text().unwrap();
                let sub: Value = if socket_io {
                    serde_json::from_str::<Value>(raw.strip_prefix("42/updates,").unwrap()).unwrap()
                        [1]
                    .clone()
                } else {
                    serde_json::from_str(raw).unwrap()
                };
                if socket_io {
                    assert_eq!(ws.read().unwrap().to_text().unwrap(), "3");
                }
                let offer = json!({"type":"firmware-offer","id":"one","session":sub["session"],"board_id":"board","boot_id":"boot",
                    "expires_at_ms":now_ms()+30000,"url":"https://updates.example/esp.bin","sha256":"a".repeat(64),"target":"esp","kind":"binary"});
                let packet = if socket_io {
                    format!("42/updates,{}", json!(["firmware-offer", offer]))
                } else {
                    offer.to_string()
                };
                for _ in 0..2 {
                    ws.send(Message::Text(packet.clone().into())).unwrap();
                }
                let _ = ws.close(None);
            });
            let s = Settings {
                endpoint: format!("ws://{addr}/events"),
                protocol: if socket_io {
                    Protocol::SocketIo
                } else {
                    Protocol::WebSocket
                },
                namespace: "/updates".into(),
                download_origin: "https://updates.example/".into(),
                ..Default::default()
            };
            let feed = Session::start(s, String::new(), "board".into(), "boot".into()).unwrap();
            let mut offers = 0;
            while let Ok(event) = feed.rx.recv_timeout(Duration::from_secs(5)) {
                match event {
                    Event::Connected => {}
                    Event::Offer(v) => {
                        assert_eq!(v.id, "one");
                        offers += 1;
                    }
                    Event::Stopped(_) => break,
                }
            }
            worker.join().unwrap();
            assert_eq!(offers, 1);
        }
    }
}
