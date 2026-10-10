use std::{
    io::{ErrorKind, Read, Write},
    sync::mpsc::{self, Receiver, Sender, TryRecvError},
    thread,
    time::{Duration, Instant},
};

use base64::{Engine, engine::general_purpose::STANDARD as BASE64};
use eframe::egui::{self, Color32, RichText, Theme};
use reqwest::blocking::Client;
use serde::Deserialize;
use tungstenite::{Message, client::IntoClientRequest};

const OP_GET_STATUS: u8 = 0x02;
const OP_BUZZER: u8 = 0x10;
const OP_DISPLAY: u8 = 0x38;
const OP_SET_MODE: u8 = 0x4b;
const OP_CELL_ACTION: u8 = 0x4c;
const OP_RESET: u8 = 0x4d;
const OP_OUTPUT_MASK: u8 = 0x4e;
const OP_BRIGHTNESS: u8 = 0x51;
const OP_PATTERN: u8 = 0x52;
const OP_NETWORK_CONFIG: u8 = 0x53;
const OP_STATUS: u8 = 0x90;

#[derive(Clone, PartialEq)]
struct Endpoint {
    base: String,
    user: String,
    password: String,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct DeviceStatus {
    #[serde(default)]
    version: String,
    #[serde(default)]
    hostname: String,
    #[serde(default)]
    ip: String,
    #[serde(default)]
    mode: String,
    #[serde(default)]
    avr_online: bool,
    #[serde(default)]
    ap_started: bool,
    #[serde(default)]
    ap_enabled: bool,
    #[serde(default)]
    ap_ip: String,
    #[serde(default)]
    station_enabled: bool,
    #[serde(default)]
    station_connected: bool,
    #[serde(default)]
    station_ssid: String,
    #[serde(default)]
    station_ip: String,
    #[serde(default)]
    station_rssi: i16,
    #[serde(default)]
    tm1637_ack: bool,
    #[serde(default)]
    programming_latch: bool,
    #[serde(default)]
    winner: u8,
    #[serde(default)]
    selected_cell: u8,
    #[serde(default)]
    brightness: u8,
    #[serde(default)]
    effect: String,
    #[serde(default)]
    pixels: u16,
    #[serde(default)]
    active_inputs: u8,
    #[serde(default)]
    input_9: bool,
    #[serde(default)]
    active_outputs: u8,
    #[serde(default)]
    esp_uart_errors: u16,
    #[serde(default)]
    avr_uart_errors: u16,
    #[serde(default)]
    usb_serial_errors: u16,
    #[serde(default)]
    ntp_synced: bool,
    #[serde(default)]
    utc_epoch: u64,
    #[serde(default)]
    websocket_clients: u8,
    #[serde(default)]
    mqtt_enabled: bool,
    #[serde(default)]
    mqtt_connected: bool,
    #[serde(default)]
    mqtt_state: i16,
    #[serde(default)]
    cloud_websocket_enabled: bool,
    #[serde(default)]
    cloud_websocket_connected: bool,
    #[serde(default)]
    cloud_error: String,
    #[serde(default)]
    link_mode: String,
    #[serde(default)]
    cells: Vec<String>,
}

enum HttpCommand {
    Status(Endpoint),
    Post(Endpoint, &'static str, Vec<(String, String)>),
}
enum WsCommand {
    Connect(Endpoint),
    Stream(bool),
    Sample(bool),
}
enum SerialCommand {
    Port(String),
    Send(u8, Vec<u8>),
}
enum Event {
    Status(DeviceStatus),
    Accepted(String),
    Failed(String),
    WebSocket(bool),
    Serial(bool, Option<DeviceStatus>),
}

fn http_request(client: &Client, command: HttpCommand) -> Result<Event, String> {
    let (endpoint, request, status_request) = match command {
        HttpCommand::Status(endpoint) => {
            let url = format!("{}/api/v1/status", endpoint.base.trim_end_matches('/'));
            let request = client.get(url);
            (endpoint, request, true)
        }
        HttpCommand::Post(endpoint, path, form) => {
            let url = format!("{}{}", endpoint.base.trim_end_matches('/'), path);
            let request = client.post(url).form(&form);
            (endpoint, request, false)
        }
    };
    let response = request
        .basic_auth(endpoint.user, Some(endpoint.password))
        .send()
        .map_err(|error| error.to_string())?;
    let code = response.status();
    if !code.is_success() {
        return Err(format!(
            "HTTP {code}: {}",
            response.text().unwrap_or_default()
        ));
    }
    if status_request {
        response
            .json::<DeviceStatus>()
            .map(Event::Status)
            .map_err(|e| e.to_string())
    } else {
        Ok(Event::Accepted(format!(
            "Device accepted the command ({code})"
        )))
    }
}

fn http_worker(commands: Receiver<HttpCommand>, events: Sender<Event>, context: egui::Context) {
    let client = Client::builder()
        .no_proxy()
        .timeout(Duration::from_secs(4))
        .build()
        .expect("HTTP client");
    while let Ok(command) = commands.recv() {
        let event = http_request(&client, command).unwrap_or_else(Event::Failed);
        if events.send(event).is_err() {
            break;
        }
        context.request_repaint();
    }
}

fn websocket_url(base: &str) -> Result<String, String> {
    let parsed = reqwest::Url::parse(base).map_err(|e| e.to_string())?;
    let scheme = if parsed.scheme() == "https" {
        "wss"
    } else {
        "ws"
    };
    let host = parsed.host_str().ok_or("endpoint has no host")?;
    let authority = match parsed.port() {
        Some(p) => format!("{host}:{p}"),
        None => host.into(),
    };
    Ok(format!("{scheme}://{authority}/ws"))
}

fn rainbow_packet(offset: u16, count: u8, commit: bool, hue: u16) -> Vec<u8> {
    let mut packet = vec![0_u8; 10 + count as usize * 3];
    packet[0..4].copy_from_slice(b"PKR1");
    packet[4] = 1;
    packet[6] = offset as u8;
    packet[7] = (offset >> 8) as u8;
    packet[8] = count;
    packet[9] = u8::from(commit);
    for i in 0..count as usize {
        let h = ((hue as usize + (offset as usize + i) * 3) % 360) as f32;
        let x = (1.0 - ((h / 60.0) % 2.0 - 1.0).abs()) * 255.0;
        let (r, g, b) = match (h / 60.0) as u8 {
            0 => (255, x as u8, 0),
            1 => (x as u8, 255, 0),
            2 => (0, 255, x as u8),
            3 => (0, x as u8, 255),
            4 => (x as u8, 0, 255),
            _ => (255, 0, x as u8),
        };
        let at = 10 + i * 3;
        packet[at..at + 3].copy_from_slice(&[r, g, b]);
    }
    packet
}

fn pixel_packet(pixels: &[u8], offset: u16, count: u8, commit: bool) -> Vec<u8> {
    let mut packet = vec![0_u8; 10 + count as usize * 3];
    packet[0..4].copy_from_slice(b"PKR1");
    packet[4] = 1;
    packet[6] = offset as u8;
    packet[7] = (offset >> 8) as u8;
    packet[8] = count;
    packet[9] = u8::from(commit);
    let start = offset as usize * 3;
    let end = start + count as usize * 3;
    packet[10..].copy_from_slice(&pixels[start..end]);
    packet
}

fn sampled_screen_packets() -> Result<(Vec<u8>, Vec<u8>), String> {
    let monitors = xcap::Monitor::all().map_err(|error| error.to_string())?;
    let monitor = monitors
        .iter()
        .find(|monitor| monitor.is_primary().unwrap_or(false))
        .or_else(|| monitors.first())
        .ok_or("no screen is available")?;
    let image = monitor.capture_image().map_err(|error| error.to_string())?;
    let mut pixels = vec![0_u8; 480 * 3];
    for y in 0..8_u32 {
        for x in 0..10_u32 {
            let sample_x = ((x * 2 + 1) * image.width() / 20).min(image.width() - 1);
            let sample_y = ((y * 2 + 1) * image.height() / 16).min(image.height() - 1);
            let color = image.get_pixel(sample_x, sample_y).0;
            let from_right = 9 - x as usize;
            let cell = from_right * 8
                + if from_right % 2 == 0 {
                    7 - y as usize
                } else {
                    y as usize
                };
            for sector in 0..6 {
                let at = (cell * 6 + sector) * 3;
                pixels[at..at + 3].copy_from_slice(&color[..3]);
            }
        }
    }
    Ok((
        pixel_packet(&pixels, 0, 240, false),
        pixel_packet(&pixels, 240, 240, true),
    ))
}

fn ws_worker(commands: Receiver<WsCommand>, events: Sender<Event>, context: egui::Context) {
    let mut endpoint: Option<Endpoint> = None;
    let mut streaming = false;
    let mut sampling = false;
    loop {
        if endpoint.is_none() {
            match commands.recv() {
                Ok(WsCommand::Connect(e)) => endpoint = Some(e),
                Ok(WsCommand::Stream(v)) => streaming = v,
                Ok(WsCommand::Sample(v)) => sampling = v,
                Err(_) => return,
            }
        }
        while let Ok(command) = commands.try_recv() {
            match command {
                WsCommand::Connect(e) => endpoint = Some(e),
                WsCommand::Stream(v) => streaming = v,
                WsCommand::Sample(v) => sampling = v,
            }
        }
        let Some(current) = endpoint.clone() else {
            continue;
        };
        let result = (|| -> Result<(), String> {
            let mut request = websocket_url(&current.base)?
                .into_client_request()
                .map_err(|e| e.to_string())?;
            let auth = format!(
                "Basic {}",
                BASE64.encode(format!("{}:{}", current.user, current.password))
            );
            request.headers_mut().insert(
                "Authorization",
                auth.parse().map_err(|_| "invalid authorization")?,
            );
            request
                .headers_mut()
                .insert("Sec-WebSocket-Protocol", "arduino".parse().unwrap());
            let (mut socket, _) = tungstenite::connect(request).map_err(|e| e.to_string())?;
            if let tungstenite::stream::MaybeTlsStream::Plain(stream) = socket.get_mut() {
                stream
                    .set_read_timeout(Some(Duration::from_millis(40)))
                    .map_err(|e| e.to_string())?;
            }
            let _ = events.send(Event::WebSocket(true));
            context.request_repaint();
            let mut hue = 0_u16;
            let mut last_stream = Instant::now() - Duration::from_millis(50);
            loop {
                match commands.try_recv() {
                    Ok(WsCommand::Connect(new_endpoint)) if new_endpoint != current => {
                        endpoint = Some(new_endpoint);
                        break;
                    }
                    Ok(WsCommand::Connect(_)) => {}
                    Ok(WsCommand::Stream(value)) => {
                        streaming = value;
                        if value {
                            sampling = false;
                        }
                    }
                    Ok(WsCommand::Sample(value)) => {
                        sampling = value;
                        if value {
                            streaming = false;
                        }
                    }
                    Err(TryRecvError::Disconnected) => return Ok(()),
                    Err(TryRecvError::Empty) => {}
                }
                if streaming && last_stream.elapsed() >= Duration::from_millis(40) {
                    socket
                        .send(Message::Binary(rainbow_packet(0, 240, false, hue).into()))
                        .map_err(|e| e.to_string())?;
                    socket
                        .send(Message::Binary(rainbow_packet(240, 240, true, hue).into()))
                        .map_err(|e| e.to_string())?;
                    hue = (hue + 4) % 360;
                    last_stream = Instant::now();
                }
                if sampling && last_stream.elapsed() >= Duration::from_millis(120) {
                    let (first, second) = sampled_screen_packets()?;
                    socket
                        .send(Message::Binary(first.into()))
                        .map_err(|e| e.to_string())?;
                    socket
                        .send(Message::Binary(second.into()))
                        .map_err(|e| e.to_string())?;
                    last_stream = Instant::now();
                }
                match socket.read() {
                    Ok(Message::Text(text)) => {
                        if let Ok(status) = serde_json::from_str::<DeviceStatus>(&text) {
                            let _ = events.send(Event::Status(status));
                            context.request_repaint();
                        }
                    }
                    Ok(Message::Close(_)) => return Err("WebSocket closed".into()),
                    Ok(_) => {}
                    Err(tungstenite::Error::Io(e))
                        if matches!(e.kind(), ErrorKind::WouldBlock | ErrorKind::TimedOut) => {}
                    Err(e) => return Err(e.to_string()),
                }
            }
            Ok(())
        })();
        let _ = events.send(Event::WebSocket(false));
        context.request_repaint();
        if let Err(error) = result {
            let _ = events.send(Event::Failed(format!("WebSocket: {error}")));
        }
        thread::sleep(Duration::from_millis(600));
    }
}

fn crc8(data: &[u8]) -> u8 {
    let mut crc = 0_u8;
    for byte in data {
        crc ^= *byte;
        for _ in 0..8 {
            crc = if crc & 0x80 != 0 {
                (crc << 1) ^ 0x07
            } else {
                crc << 1
            };
        }
    }
    crc
}
fn cobs_encode(input: &[u8]) -> Vec<u8> {
    let mut out = vec![0_u8];
    let mut code_at = 0;
    let mut code = 1_u8;
    for byte in input {
        if *byte == 0 {
            out[code_at] = code;
            code_at = out.len();
            out.push(0);
            code = 1;
        } else {
            out.push(*byte);
            code += 1;
            if code == 0xff {
                out[code_at] = code;
                code_at = out.len();
                out.push(0);
                code = 1;
            }
        }
    }
    out[code_at] = code;
    out.push(0);
    out
}
fn cobs_decode(input: &[u8]) -> Option<Vec<u8>> {
    let mut out = Vec::new();
    let mut at = 0;
    while at < input.len() {
        let code = *input.get(at)? as usize;
        if code == 0 || at + code > input.len() + 1 {
            return None;
        }
        at += 1;
        for _ in 1..code {
            out.push(*input.get(at)?);
            at += 1;
        }
        if code != 0xff && at < input.len() {
            out.push(0);
        }
    }
    Some(out)
}
fn wire_frame(opcode: u8, sequence: u8, payload: &[u8]) -> Vec<u8> {
    let mut raw = vec![0xa5, 1, opcode, sequence, payload.len() as u8];
    raw.extend_from_slice(payload);
    raw.push(crc8(&raw));
    cobs_encode(&raw)
}
fn decode_frame(encoded: &[u8]) -> Option<(u8, Vec<u8>)> {
    let raw = cobs_decode(encoded)?;
    if raw.len() < 6
        || raw[0] != 0xa5
        || raw[1] != 1
        || raw.len() != raw[4] as usize + 6
        || crc8(&raw[..raw.len() - 1]) != raw[raw.len() - 1]
    {
        return None;
    }
    Some((raw[2], raw[5..raw.len() - 1].to_vec()))
}
fn serial_status(payload: &[u8]) -> Option<DeviceStatus> {
    if payload.len() < 19 {
        return None;
    }
    Some(DeviceStatus {
        mode: ["idle", "cell", "first-wins"]
            .get(payload[0] as usize)
            .unwrap_or(&"unknown")
            .to_string(),
        winner: payload[1],
        selected_cell: payload[2],
        brightness: payload[3],
        effect: ["none", "rainbow", "fire"]
            .get(payload[4] as usize)
            .unwrap_or(&"unknown")
            .to_string(),
        avr_online: payload[5] != 0,
        tm1637_ack: payload[6] != 0,
        programming_latch: payload[7] != 0,
        active_inputs: payload[8],
        input_9: payload[9] != 0,
        active_outputs: payload[10],
        esp_uart_errors: u16::from_le_bytes([payload[11], payload[12]]),
        avr_uart_errors: u16::from_le_bytes([payload[13], payload[14]]),
        station_connected: payload[15] != 0,
        ap_started: payload[16] != 0,
        ntp_synced: payload[17] != 0,
        link_mode: if payload[18] != 0 {
            "busy-wire"
        } else {
            "polling"
        }
        .into(),
        ..Default::default()
    })
}

fn serial_worker(commands: Receiver<SerialCommand>, events: Sender<Event>, context: egui::Context) {
    let mut port_name = String::new();
    let mut sequence = 1_u8;
    loop {
        if port_name.is_empty() {
            match commands.recv() {
                Ok(SerialCommand::Port(p)) => port_name = p,
                Ok(_) => {}
                Err(_) => return,
            }
        }
        if port_name.is_empty() {
            continue;
        }
        let opened = serialport::new(&port_name, 115_200)
            .timeout(Duration::from_millis(60))
            .open();
        let Ok(mut port) = opened else {
            let _ = events.send(Event::Serial(false, None));
            thread::sleep(Duration::from_secs(1));
            while let Ok(c) = commands.try_recv() {
                if let SerialCommand::Port(p) = c {
                    port_name = p
                }
            }
            continue;
        };
        let _ = events.send(Event::Serial(true, None));
        context.request_repaint();
        let mut encoded = Vec::new();
        let mut last_poll = Instant::now() - Duration::from_secs(1);
        'connected: loop {
            while let Ok(command) = commands.try_recv() {
                match command {
                    SerialCommand::Port(p) if p != port_name => {
                        port_name = p;
                        break 'connected;
                    }
                    SerialCommand::Port(_) => {}
                    SerialCommand::Send(op, payload) => {
                        let _ = port.write_all(&wire_frame(op, sequence, &payload));
                        sequence = sequence.wrapping_add(1);
                    }
                }
            }
            if last_poll.elapsed() >= Duration::from_millis(750) {
                let _ = port.write_all(&wire_frame(OP_GET_STATUS, sequence, &[]));
                sequence = sequence.wrapping_add(1);
                last_poll = Instant::now();
            }
            let mut bytes = [0_u8; 128];
            match port.read(&mut bytes) {
                Ok(n) => {
                    for byte in &bytes[..n] {
                        if *byte == 0 {
                            if let Some((op, payload)) = decode_frame(&encoded) {
                                if op == OP_STATUS {
                                    let _ =
                                        events.send(Event::Serial(true, serial_status(&payload)));
                                    context.request_repaint();
                                }
                            }
                            encoded.clear();
                        } else if encoded.len() < 80 {
                            encoded.push(*byte)
                        } else {
                            encoded.clear()
                        }
                    }
                }
                Err(e) if matches!(e.kind(), ErrorKind::TimedOut | ErrorKind::WouldBlock) => {}
                Err(_) => break,
            }
        }
        let _ = events.send(Event::Serial(false, None));
        context.request_repaint();
    }
}

#[derive(Clone, Copy, PartialEq)]
enum Transport {
    WiFi,
    Serial,
    Both,
}
struct PakarControllerApp {
    base: String,
    user: String,
    password: String,
    serial_port: String,
    transport: Transport,
    status: Option<DeviceStatus>,
    serial_status: Option<DeviceStatus>,
    brightness: u8,
    pattern: String,
    display_value: i16,
    buzzer_frequency: u16,
    buzzer_duration: u16,
    output_mask: u8,
    wifi_ssid: String,
    wifi_password: String,
    station_enabled: bool,
    ap_enabled: bool,
    mqtt_enabled: bool,
    mqtt_tls: bool,
    mqtt_host: String,
    mqtt_port: u16,
    mqtt_user: String,
    mqtt_password: String,
    mqtt_topic: String,
    cloud_ws_enabled: bool,
    cloud_ws_tls: bool,
    cloud_ws_host: String,
    cloud_ws_port: u16,
    cloud_ws_path: String,
    cloud_ws_token: String,
    telemetry_seconds: u16,
    message: String,
    http_tx: Sender<HttpCommand>,
    ws_tx: Sender<WsCommand>,
    serial_tx: Sender<SerialCommand>,
    event_rx: Receiver<Event>,
    last_poll: Instant,
    last_ws_endpoint: Option<Endpoint>,
    last_serial_port: String,
    websocket_live: bool,
    serial_live: bool,
    streaming: bool,
    sampling: bool,
}

impl PakarControllerApp {
    fn new(context: &eframe::CreationContext<'_>) -> Self {
        install_pealayer_style(&context.egui_ctx);
        let (http_tx, http_rx) = mpsc::channel();
        let (ws_tx, ws_rx) = mpsc::channel();
        let (serial_tx, serial_rx) = mpsc::channel();
        let (event_tx, event_rx) = mpsc::channel();
        let ctx = context.egui_ctx.clone();
        let events = event_tx.clone();
        thread::Builder::new()
            .name("pakar-http".into())
            .spawn(move || http_worker(http_rx, events, ctx))
            .unwrap();
        let ctx = context.egui_ctx.clone();
        let events = event_tx.clone();
        thread::Builder::new()
            .name("pakar-websocket".into())
            .spawn(move || ws_worker(ws_rx, events, ctx))
            .unwrap();
        let ctx = context.egui_ctx.clone();
        thread::Builder::new()
            .name("pakar-serial".into())
            .spawn(move || serial_worker(serial_rx, event_tx, ctx))
            .unwrap();
        Self {
            base: std::env::var("PAKAR_ENDPOINT").unwrap_or_else(|_| "http://192.168.4.1".into()),
            user: std::env::var("PAKAR_USER").unwrap_or_else(|_| "admin".into()),
            password: std::env::var("PAKAR_PASSWORD").unwrap_or_else(|_| "pakarcontroller".into()),
            serial_port: std::env::var("PAKAR_SERIAL").unwrap_or_default(),
            transport: Transport::Both,
            status: None,
            serial_status: None,
            brightness: 255,
            pattern: "none".into(),
            display_value: 1,
            buzzer_frequency: 1032,
            buzzer_duration: 100,
            output_mask: 0,
            wifi_ssid: "DRS Tether".into(),
            wifi_password: String::new(),
            station_enabled: true,
            ap_enabled: true,
            mqtt_enabled: true,
            mqtt_tls: true,
            mqtt_host: "mqtt.digitalogic.ir".into(),
            mqtt_port: 8883,
            mqtt_user: "pakarcontroller-b45634".into(),
            mqtt_password: String::new(),
            mqtt_topic: "pakarcontroller".into(),
            cloud_ws_enabled: false,
            cloud_ws_tls: true,
            cloud_ws_host: String::new(),
            cloud_ws_port: 443,
            cloud_ws_path: "/".into(),
            cloud_ws_token: String::new(),
            telemetry_seconds: 5,
            message: "Connecting to PakarController".into(),
            http_tx,
            ws_tx,
            serial_tx,
            event_rx,
            last_poll: Instant::now() - Duration::from_secs(2),
            last_ws_endpoint: None,
            last_serial_port: String::new(),
            websocket_live: false,
            serial_live: false,
            streaming: false,
            sampling: false,
        }
    }
    fn endpoint(&self) -> Endpoint {
        Endpoint {
            base: self.base.clone(),
            user: self.user.clone(),
            password: self.password.clone(),
        }
    }
    fn post(&mut self, path: &'static str, form: Vec<(String, String)>) {
        if self.transport != Transport::Serial {
            let _ = self
                .http_tx
                .send(HttpCommand::Post(self.endpoint(), path, form));
        }
    }
    fn serial(&self, op: u8, payload: Vec<u8>) {
        if self.transport != Transport::WiFi {
            let _ = self.serial_tx.send(SerialCommand::Send(op, payload));
        }
    }
    fn action(
        &mut self,
        path: &'static str,
        form: Vec<(String, String)>,
        op: u8,
        payload: Vec<u8>,
    ) {
        self.post(path, form);
        self.serial(op, payload);
    }
    fn apply_network(&mut self) {
        if !self.station_enabled && !self.ap_enabled {
            self.message = "Keep station or recovery AP enabled".into();
            return;
        }
        self.post(
            "/api/v1/network",
            vec![
                ("station_enabled".into(), self.station_enabled.to_string()),
                ("ap_enabled".into(), self.ap_enabled.to_string()),
                ("ssid".into(), self.wifi_ssid.clone()),
                ("password".into(), self.wifi_password.clone()),
            ],
        );
        if self.transport != Transport::WiFi {
            self.serial(OP_NETWORK_CONFIG, vec![0]);
            for chunk in self.wifi_ssid.as_bytes().chunks(47) {
                let mut payload = vec![1];
                payload.extend_from_slice(chunk);
                self.serial(OP_NETWORK_CONFIG, payload);
            }
            for chunk in self.wifi_password.as_bytes().chunks(47) {
                let mut payload = vec![2];
                payload.extend_from_slice(chunk);
                self.serial(OP_NETWORK_CONFIG, payload);
            }
            self.serial(
                OP_NETWORK_CONFIG,
                vec![3, u8::from(self.station_enabled), u8::from(self.ap_enabled)],
            );
        }
        self.message = "Network configuration submitted".into();
    }
    fn apply_cloud(&mut self) {
        if self.transport == Transport::Serial {
            self.message = "Cloud settings require the authenticated HTTP path".into();
            return;
        }
        let mut form = vec![
            ("mqtt_enabled".into(), self.mqtt_enabled.to_string()),
            ("mqtt_tls".into(), self.mqtt_tls.to_string()),
            ("mqtt_host".into(), self.mqtt_host.clone()),
            ("mqtt_port".into(), self.mqtt_port.to_string()),
            ("mqtt_user".into(), self.mqtt_user.clone()),
            ("mqtt_base_topic".into(), self.mqtt_topic.clone()),
            (
                "websocket_enabled".into(),
                self.cloud_ws_enabled.to_string(),
            ),
            ("websocket_tls".into(), self.cloud_ws_tls.to_string()),
            ("websocket_host".into(), self.cloud_ws_host.clone()),
            ("websocket_port".into(), self.cloud_ws_port.to_string()),
            ("websocket_path".into(), self.cloud_ws_path.clone()),
            (
                "telemetry_seconds".into(),
                self.telemetry_seconds.to_string(),
            ),
        ];
        if !self.mqtt_password.is_empty() {
            form.push(("mqtt_password".into(), self.mqtt_password.clone()));
        }
        if !self.cloud_ws_token.is_empty() {
            form.push(("websocket_token".into(), self.cloud_ws_token.clone()));
        }
        self.post("/api/v1/cloud", form);
        self.mqtt_password.clear();
        self.cloud_ws_token.clear();
        self.message = "Cloud configuration submitted".into();
    }
    fn poll(&mut self) {
        let endpoint = self.endpoint();
        if self.last_ws_endpoint.as_ref() != Some(&endpoint) {
            let _ = self.ws_tx.send(WsCommand::Connect(endpoint.clone()));
            self.last_ws_endpoint = Some(endpoint);
        }
        if self.last_serial_port != self.serial_port {
            let _ = self
                .serial_tx
                .send(SerialCommand::Port(self.serial_port.clone()));
            self.last_serial_port = self.serial_port.clone();
        }
        if self.last_poll.elapsed() >= Duration::from_secs(2) {
            self.last_poll = Instant::now();
            let _ = self.http_tx.send(HttpCommand::Status(self.endpoint()));
        }
        while let Ok(event) = self.event_rx.try_recv() {
            match event {
                Event::Status(s) => {
                    if !s.hostname.is_empty() {
                        self.brightness = s.brightness;
                        self.pattern = s.effect.clone();
                        self.output_mask = s.active_outputs;
                        self.status = Some(s)
                    }
                }
                Event::Accepted(m) => self.message = m,
                Event::Failed(e) => self.message = e,
                Event::WebSocket(v) => self.websocket_live = v,
                Event::Serial(v, s) => {
                    self.serial_live = v;
                    if let Some(s) = s {
                        self.serial_status = Some(s)
                    }
                }
            }
        }
    }
    fn cell_color(s: &str) -> Color32 {
        match s {
            "red" => Color32::from_rgb(218, 54, 58),
            "off" => Color32::from_rgb(45, 48, 54),
            _ => Color32::from_rgb(235, 235, 225),
        }
    }
    fn next_cell_state(s: &str) -> (&'static str, u8) {
        match s {
            "white" => ("red", 1),
            "red" => ("off", 2),
            _ => ("white", 0),
        }
    }
}

impl eframe::App for PakarControllerApp {
    fn ui(&mut self, ui: &mut egui::Ui, _: &mut eframe::Frame) {
        self.poll();
        ui.ctx().request_repaint_after(Duration::from_millis(150));
        ui.horizontal_wrapped(|ui| {
            ui.heading("PakarController");
            ui.label("Wi-Fi");
            ui.text_edit_singleline(&mut self.base);
            ui.label("USB serial");
            ui.add(egui::TextEdit::singleline(&mut self.serial_port).hint_text("COM9"));
            egui::ComboBox::from_label("Commands")
                .selected_text(match self.transport {
                    Transport::WiFi => "Wi-Fi",
                    Transport::Serial => "Serial",
                    Transport::Both => "Both",
                })
                .show_ui(ui, |ui| {
                    ui.selectable_value(&mut self.transport, Transport::Both, "Both");
                    ui.selectable_value(&mut self.transport, Transport::WiFi, "Wi-Fi");
                    ui.selectable_value(&mut self.transport, Transport::Serial, "Serial");
                });
        });
        ui.horizontal_wrapped(|ui| {
            ui.label(format!(
                "WebSocket {}",
                if self.websocket_live {
                    "live"
                } else {
                    "offline"
                }
            ));
            ui.label(format!(
                "USB {}",
                if self.serial_live { "live" } else { "offline" }
            ));
            if ui
                .button(if self.streaming {
                    "Stop streamed rainbow"
                } else {
                    "Stream rainbow"
                })
                .clicked()
            {
                self.streaming = !self.streaming;
                if self.streaming {
                    self.sampling = false;
                }
                let _ = self.ws_tx.send(WsCommand::Stream(self.streaming));
            }
            if ui
                .button(if self.sampling {
                    "Stop screen sampler"
                } else {
                    "Sample primary screen"
                })
                .clicked()
            {
                self.sampling = !self.sampling;
                if self.sampling {
                    self.streaming = false;
                }
                let _ = self.ws_tx.send(WsCommand::Sample(self.sampling));
            }
            ui.label(&self.message);
        });
        ui.horizontal_wrapped(|ui| {
            ui.label("Wi-Fi SSID");
            ui.text_edit_singleline(&mut self.wifi_ssid);
            ui.label("Password");
            ui.add(egui::TextEdit::singleline(&mut self.wifi_password).password(true));
            ui.checkbox(&mut self.station_enabled, "Station");
            ui.checkbox(&mut self.ap_enabled, "Recovery AP");
            if ui.button("Apply network").clicked() {
                self.apply_network();
            }
        });
        egui::CollapsingHeader::new("Cloud MQTT and outbound WebSocket")
            .default_open(false)
            .show(ui, |ui| {
                ui.horizontal_wrapped(|ui| {
                    ui.checkbox(&mut self.mqtt_enabled, "MQTT");
                    ui.checkbox(&mut self.mqtt_tls, "TLS");
                    ui.label("Host");
                    ui.text_edit_singleline(&mut self.mqtt_host);
                    ui.add(egui::DragValue::new(&mut self.mqtt_port).range(1..=65535));
                    ui.label("Topic");
                    ui.text_edit_singleline(&mut self.mqtt_topic);
                });
                ui.horizontal_wrapped(|ui| {
                    ui.label("MQTT user");
                    ui.text_edit_singleline(&mut self.mqtt_user);
                    ui.label("Password");
                    ui.add(
                        egui::TextEdit::singleline(&mut self.mqtt_password)
                            .password(true)
                            .hint_text("blank keeps stored"),
                    );
                });
                ui.horizontal_wrapped(|ui| {
                    ui.checkbox(&mut self.cloud_ws_enabled, "Outbound WebSocket");
                    ui.checkbox(&mut self.cloud_ws_tls, "TLS");
                    ui.label("Host");
                    ui.text_edit_singleline(&mut self.cloud_ws_host);
                    ui.add(egui::DragValue::new(&mut self.cloud_ws_port).range(1..=65535));
                    ui.label("Path");
                    ui.text_edit_singleline(&mut self.cloud_ws_path);
                });
                ui.horizontal_wrapped(|ui| {
                    ui.label("Bearer token");
                    ui.add(
                        egui::TextEdit::singleline(&mut self.cloud_ws_token)
                            .password(true)
                            .hint_text("blank keeps stored"),
                    );
                    ui.label("Telemetry seconds");
                    ui.add(egui::DragValue::new(&mut self.telemetry_seconds).range(1..=3600));
                    if ui.button("Apply cloud").clicked() {
                        self.apply_cloud();
                    }
                });
            });
        ui.separator();
        ui.horizontal_wrapped(|ui| {
            for (label, name, value) in [
                ("Idle", "idle", 0_u8),
                ("80 cells", "cell", 1),
                ("First wins", "first-wins", 2),
                ("Snake", "snake", 3),
            ] {
                if ui.button(label).clicked() {
                    self.action(
                        "/api/v1/mode",
                        vec![("value".into(), name.into())],
                        OP_SET_MODE,
                        vec![value],
                    );
                }
            }
            if ui.button("Reset").clicked() {
                self.action("/api/v1/game/reset", vec![], OP_RESET, vec![]);
            }
            for (label, direction) in [("↑", "up"), ("←", "left"), ("↓", "down"), ("→", "right")]
            {
                if ui.button(label).clicked() {
                    self.post(
                        "/api/v1/snake/direction",
                        vec![("value".into(), direction.into())],
                    );
                }
            }
            if ui
                .add(egui::Slider::new(&mut self.brightness, 1..=255).text("Brightness"))
                .changed()
            {
                self.action(
                    "/api/v1/brightness",
                    vec![("value".into(), self.brightness.to_string())],
                    OP_BRIGHTNESS,
                    vec![self.brightness],
                );
            }
            egui::ComboBox::from_label("Effect")
                .selected_text(&self.pattern)
                .show_ui(ui, |ui| {
                    for name in ["none", "rainbow", "fire"] {
                        ui.selectable_value(&mut self.pattern, name.into(), name);
                    }
                });
            if ui.button("Apply").clicked() {
                let p = match self.pattern.as_str() {
                    "rainbow" => 1,
                    "fire" => 2,
                    _ => 0,
                };
                self.action(
                    "/api/v1/effect",
                    vec![("value".into(), self.pattern.clone())],
                    OP_PATTERN,
                    vec![p],
                );
            }
        });
        ui.horizontal_wrapped(|ui| {
            ui.label("TM1637");
            ui.add(egui::DragValue::new(&mut self.display_value).range(-999..=9999));
            if ui.button("Show").clicked() {
                self.action(
                    "/api/v1/display",
                    vec![("value".into(), self.display_value.to_string())],
                    OP_DISPLAY,
                    self.display_value.to_le_bytes().to_vec(),
                );
            }
            ui.label("Buzzer Hz/ms");
            ui.add(egui::DragValue::new(&mut self.buzzer_frequency).range(20..=12000));
            ui.add(egui::DragValue::new(&mut self.buzzer_duration).range(1..=5000));
            if ui.button("Play").clicked() {
                let mut p = self.buzzer_frequency.to_le_bytes().to_vec();
                p.extend(self.buzzer_duration.to_le_bytes());
                self.action(
                    "/api/v1/buzzer",
                    vec![
                        ("frequency".into(), self.buzzer_frequency.to_string()),
                        ("duration_ms".into(), self.buzzer_duration.to_string()),
                    ],
                    OP_BUZZER,
                    p,
                );
            }
            ui.label("Outputs");
            for bit in 0..8 {
                let mut on = self.output_mask & (1 << bit) != 0;
                if ui.checkbox(&mut on, (bit + 1).to_string()).changed() {
                    if on {
                        self.output_mask |= 1 << bit
                    } else {
                        self.output_mask &= !(1 << bit)
                    }
                    self.action(
                        "/api/v1/outputs",
                        vec![("mask".into(), self.output_mask.to_string())],
                        OP_OUTPUT_MASK,
                        vec![self.output_mask],
                    );
                }
            }
        });
        ui.separator();
        let snapshot = self.status.clone();
        if let Some(s) = snapshot {
            ui.horizontal_wrapped(|ui| {
                ui.label(RichText::new(format!("{} v{}", s.hostname, s.version)).strong());
                ui.label(format!(
                    "{} · AP {} · NTP {} · WS clients {}",
                    if s.station_connected {
                        format!("{} / {} dBm", s.station_ip, s.station_rssi)
                    } else {
                        "station offline".into()
                    },
                    if s.ap_started { &s.ap_ip } else { "off" },
                    if s.ntp_synced {
                        s.utc_epoch.to_string()
                    } else {
                        "waiting".into()
                    },
                    s.websocket_clients
                ));
                ui.colored_label(
                    if s.avr_online {
                        Color32::LIGHT_GREEN
                    } else {
                        Color32::LIGHT_RED
                    },
                    if s.avr_online {
                        "AVR online"
                    } else {
                        "AVR offline"
                    },
                );
                ui.colored_label(
                    if s.tm1637_ack {
                        Color32::LIGHT_GREEN
                    } else {
                        Color32::LIGHT_RED
                    },
                    if s.tm1637_ack {
                        "TM1637 ready"
                    } else {
                        "TM1637 missing"
                    },
                );
                ui.label(format!(
                    "{} · inputs {:02X}+{} · outputs {:02X} · UART {}/{}/USB {}",
                    s.link_mode,
                    s.active_inputs,
                    u8::from(s.input_9),
                    s.active_outputs,
                    s.esp_uart_errors,
                    s.avr_uart_errors,
                    s.usb_serial_errors
                ));
                ui.colored_label(
                    if s.mqtt_connected {
                        Color32::LIGHT_GREEN
                    } else if s.mqtt_enabled {
                        Color32::GOLD
                    } else {
                        Color32::GRAY
                    },
                    if s.mqtt_connected {
                        "MQTT connected".into()
                    } else if s.mqtt_enabled {
                        format!("MQTT offline ({})", s.mqtt_state)
                    } else {
                        "MQTT disabled".into()
                    },
                );
                ui.colored_label(
                    if s.cloud_websocket_connected {
                        Color32::LIGHT_GREEN
                    } else if s.cloud_websocket_enabled {
                        Color32::GOLD
                    } else {
                        Color32::GRAY
                    },
                    if s.cloud_websocket_connected {
                        "Cloud WS connected"
                    } else if s.cloud_websocket_enabled {
                        "Cloud WS offline"
                    } else {
                        "Cloud WS disabled"
                    },
                );
                if !s.cloud_error.is_empty() {
                    ui.colored_label(Color32::LIGHT_RED, &s.cloud_error);
                }
                if s.winner != 0 {
                    ui.colored_label(Color32::GOLD, format!("Winner {}", s.winner));
                }
            });
            egui::Grid::new("cells").spacing([5.0, 5.0]).show(ui, |ui| {
                for visual_row in 0..9_i32 {
                    for from_left in 0..10_i32 {
                        let from_right = 9 - from_left;
                        let offset = from_right % 2 == 0;
                        let y = if offset { visual_row - 1 } else { visual_row };
                        if !(0..8).contains(&y) {
                            ui.add_space(48.0);
                            continue;
                        }
                        let id = from_right * 8 + if from_right % 2 == 0 { 8 - y } else { y + 1 };
                        let index = (id - 1) as usize;
                        let state = s.cells.get(index).map(String::as_str).unwrap_or("off");
                        let (next, value) = Self::next_cell_state(state);
                        let color = if state == "white" {
                            Color32::BLACK
                        } else {
                            Color32::WHITE
                        };
                        if ui
                            .add_sized(
                                [48.0, 34.0],
                                egui::Button::new(RichText::new(id.to_string()).color(color))
                                    .fill(Self::cell_color(state)),
                            )
                            .clicked()
                        {
                            self.action(
                                "/api/v1/cell",
                                vec![
                                    ("id".into(), id.to_string()),
                                    ("state".into(), next.into()),
                                    ("effect".into(), "fade".into()),
                                ],
                                OP_CELL_ACTION,
                                vec![id as u8, value, 1],
                            );
                        }
                    }
                    ui.end_row();
                }
            });
        } else {
            ui.label("Waiting for a live device response.");
        }
    }
}

fn install_pealayer_style(context: &egui::Context) {
    context.set_theme(egui::ThemePreference::System);
    let accent = Color32::from_rgb(51, 132, 255);
    let mut dark = egui::Visuals::dark();
    dark.panel_fill = Color32::from_rgb(32, 32, 32);
    dark.selection.bg_fill = accent;
    let mut light = egui::Visuals::light();
    light.panel_fill = Color32::from_rgb(243, 243, 243);
    light.selection.bg_fill = accent;
    context.set_visuals_of(Theme::Dark, dark);
    context.set_visuals_of(Theme::Light, light);
}

fn environment_endpoint() -> Endpoint {
    Endpoint {
        base: std::env::var("PAKAR_ENDPOINT").unwrap_or_else(|_| "http://192.168.4.1".into()),
        user: std::env::var("PAKAR_USER").unwrap_or_else(|_| "admin".into()),
        password: std::env::var("PAKAR_PASSWORD").unwrap_or_else(|_| "pakarcontroller".into()),
    }
}

fn blocking_status(client: &Client, endpoint: &Endpoint) -> Result<DeviceStatus, String> {
    let response = client
        .get(format!(
            "{}/api/v1/status",
            endpoint.base.trim_end_matches('/')
        ))
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .send()
        .map_err(|error| error.to_string())?;
    if !response.status().is_success() {
        return Err(format!(
            "HTTP {}: {}",
            response.status(),
            response.text().unwrap_or_default()
        ));
    }
    response.json().map_err(|error| error.to_string())
}

fn print_text_status(status: &DeviceStatus, table: bool) {
    println!(
        "{} {} | AVR {} | TM1637 {} | {} | MQTT {} | NTP {} | brightness {} | effect {}",
        status.hostname,
        status.station_ip,
        if status.avr_online {
            "online"
        } else {
            "offline"
        },
        if status.tm1637_ack {
            "ready"
        } else {
            "missing"
        },
        status.mode,
        if status.mqtt_connected {
            "connected"
        } else {
            "offline"
        },
        if status.ntp_synced {
            "synced"
        } else {
            "waiting"
        },
        status.brightness,
        status.effect
    );
    println!(
        "inputs {:02X}+{} outputs {:02X} winner {} link {} UART {}/{}/{}",
        status.active_inputs,
        u8::from(status.input_9),
        status.active_outputs,
        status.winner,
        status.link_mode,
        status.esp_uart_errors,
        status.avr_uart_errors,
        status.usb_serial_errors
    );
    if !table {
        return;
    }
    for visual_row in 0..9_i32 {
        for from_left in 0..10_i32 {
            let from_right = 9 - from_left;
            let y = if from_right % 2 == 0 {
                visual_row - 1
            } else {
                visual_row
            };
            if !(0..8).contains(&y) {
                print!("     ");
                continue;
            }
            let id = from_right * 8 + if from_right % 2 == 0 { 8 - y } else { y + 1 };
            let mark = match status.cells.get((id - 1) as usize).map(String::as_str) {
                Some("white") => 'W',
                Some("red") => 'R',
                _ => '.',
            };
            print!("{id:02}{mark}  ");
        }
        println!();
    }
}

fn text_command(client: &Client, endpoint: &Endpoint, words: &[&str]) -> Result<(), String> {
    if words.is_empty() || words[0] == "status" {
        return blocking_status(client, endpoint).map(|status| print_text_status(&status, true));
    }
    let (path, form): (&str, Vec<(&str, &str)>) =
        match words {
            ["mode", value] => ("/api/v1/mode", vec![("value", value)]),
            ["cell", id, state] => (
                "/api/v1/cell",
                vec![("id", id), ("state", state), ("effect", "fade")],
            ),
            ["cell", id, state, effect] => (
                "/api/v1/cell",
                vec![("id", id), ("state", state), ("effect", effect)],
            ),
            ["brightness", value] => ("/api/v1/brightness", vec![("value", value)]),
            ["effect", value] => ("/api/v1/effect", vec![("value", value)]),
            ["display", value] => ("/api/v1/display", vec![("value", value)]),
            ["audio", cue] => ("/api/v1/audio", vec![("cue", cue)]),
            ["snake", value] => ("/api/v1/snake/direction", vec![("value", value)]),
            ["reset"] => ("/api/v1/game/reset", vec![]),
            _ => return Err(
                "commands: status, mode, cell, brightness, effect, display, audio, snake, reset"
                    .into(),
            ),
        };
    let response = client
        .post(format!("{}{}", endpoint.base.trim_end_matches('/'), path))
        .basic_auth(&endpoint.user, Some(&endpoint.password))
        .form(&form)
        .send()
        .map_err(|error| error.to_string())?;
    if !response.status().is_success() {
        return Err(format!(
            "HTTP {}: {}",
            response.status(),
            response.text().unwrap_or_default()
        ));
    }
    println!("accepted");
    Ok(())
}

fn run_text_interface(arguments: &[String]) -> Result<bool, String> {
    if !matches!(
        arguments.get(1).map(String::as_str),
        Some("--status" | "--command" | "--tui")
    ) {
        return Ok(false);
    }
    let endpoint = environment_endpoint();
    let client = Client::builder()
        .no_proxy()
        .timeout(Duration::from_secs(15))
        .build()
        .map_err(|error| error.to_string())?;
    match arguments[1].as_str() {
        "--status" => text_command(&client, &endpoint, &["status"]),
        "--command" => {
            let words = arguments[2..]
                .iter()
                .map(String::as_str)
                .collect::<Vec<_>>();
            text_command(&client, &endpoint, &words)
        }
        "--tui" => {
            println!("PakarController TUI. Type help or quit.");
            loop {
                match blocking_status(&client, &endpoint) {
                    Ok(status) => print_text_status(&status, true),
                    Err(error) => eprintln!("status: {error}"),
                }
                print!("pakar> ");
                std::io::stdout()
                    .flush()
                    .map_err(|error| error.to_string())?;
                let mut line = String::new();
                if std::io::stdin()
                    .read_line(&mut line)
                    .map_err(|error| error.to_string())?
                    == 0
                {
                    break;
                }
                let words = line.split_whitespace().collect::<Vec<_>>();
                if matches!(words.first(), Some(&"quit" | &"exit")) {
                    break;
                }
                if matches!(words.first(), Some(&"help")) {
                    println!(
                        "status | mode <...> | cell <id> <state> [effect] | brightness <n> | effect <name> | display <n> | audio <cue> | snake <direction> | reset | quit"
                    );
                    continue;
                }
                if let Err(error) = text_command(&client, &endpoint, &words) {
                    eprintln!("{error}");
                }
            }
            Ok(())
        }
        _ => unreachable!(),
    }?;
    Ok(true)
}

fn main() -> eframe::Result {
    let arguments = std::env::args().collect::<Vec<_>>();
    match run_text_interface(&arguments) {
        Ok(true) => return Ok(()),
        Ok(false) => {}
        Err(error) => {
            eprintln!("PakarController: {error}");
            std::process::exit(2);
        }
    }
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default().with_inner_size([1180.0, 780.0]),
        ..Default::default()
    };
    eframe::run_native(
        "PakarController",
        options,
        Box::new(|c| Ok(Box::new(PakarControllerApp::new(c)))),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn wire_round_trip() {
        let encoded = wire_frame(OP_CELL_ACTION, 7, &[80, 2, 1]);
        let (op, payload) = decode_frame(&encoded[..encoded.len() - 1]).unwrap();
        assert_eq!(op, OP_CELL_ACTION);
        assert_eq!(payload, vec![80, 2, 1]);
    }
    #[test]
    fn streaming_packet_bounds() {
        let p = rainbow_packet(240, 240, true, 20);
        assert_eq!(p.len(), 730);
        assert_eq!(&p[..4], b"PKR1");
        assert_eq!(p[9], 1);
    }
    #[test]
    fn cell_states_cycle() {
        assert_eq!(PakarControllerApp::next_cell_state("white"), ("red", 1));
        assert_eq!(PakarControllerApp::next_cell_state("red"), ("off", 2));
    }
}
